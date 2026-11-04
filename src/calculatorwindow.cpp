// SPDX-FileCopyrightText: 2026 LingmoOS Contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "calculatorwindow.h"

#include "modes/date_page.h"
#include "modes/graph_page.h"
#include "modes/programmer_page.h"
#include "modes/scientific_page.h"
#include "modes/standard_page.h"

#include "base/dbus.h"
#include "base/event_loop.h"
#include "base/i18n.h"
#include "base/paths.h"
#include "base/settings.h"
#include "ui/label.h"
#include "ui/layout.h"
#include "ui/painter.h"
#include "ui/theme.h"
#include "ui/view.h"
#include "ui/window.h"
#include "ui/window_header.h"

#include <functional>
#include <map>
#include <string>
#include <vector>

using namespace Lingmo::ui;
using Lingmo::i18n::tr;

namespace calculator {

namespace {

constexpr double kHeaderHeight = 44;

static constexpr double kModeWidths[]  = { 340, 520, 480, 420, 600 };
static constexpr double kModeHeights[] = { 664, 664, 624, 464, 584 };

// ─── Simple window root ───────────────────────────────────────────────────────

class CalcRoot : public View
{
public:
    CalcRoot()
        : m_header(addChild<WindowHeader>(kHeaderHeight))
        , m_content(addChild<View>())
    {
        setName("calculatorRoot");
        m_header->setName("header");
        m_header->setButtonsCentered(true);
        m_header->setButtonMargin(4);
        m_content->setName("content");
    }

    WindowHeader* header() const { return m_header; }
    View* content() const { return m_content; }

protected:
    void paint(Painter& painter) override
    {
        painter.fillRect(localRect(), Theme::current().background);
    }

    void layout() override
    {
        const RectF rect = localRect();
        m_header->setGeometry({0, 0, rect.width, kHeaderHeight});
        m_content->setGeometry({0, kHeaderHeight, rect.width,
                                std::max(0.0, rect.height - kHeaderHeight)});
        const RectF page{0, 0, m_content->geometry().width, m_content->geometry().height};
        for (const auto& child : m_content->children())
            child->setGeometry(page);
    }

private:
    WindowHeader* m_header;
    View* m_content;
};

} // anonymous namespace

// ─── Global menu exporter (com.canonical.dbusmenu) ───────────────────────────
//
// Exports the mode-switcher submenu to the compositor's menu bar through
// the com.canonical.AppMenu.Registrar protocol.  The compositor matches the
// registration to this process by PID, so the window id passed to
// RegisterWindow can be any non-zero value (0 is reserved for the desktop's
// global menu exported by the file manager).

class GlobalMenuExporter
{
public:
    struct Entry {
        std::string label;
        bool separator  = false;
        bool enabled    = true;
        bool checked    = false;
        std::function<void()> action;
        std::vector<Entry> children;
    };

    explicit GlobalMenuExporter(Lingmo::base::dbus::Connection* bus,
                                std::function<std::vector<Entry>()> menu)
        : m_bus(bus)
        , m_menu(std::move(menu))
        , m_object(bus, kMenuPath, kInterface)
    {
        using namespace Lingmo::base::dbus;

        rebuild();

        m_object.addProperty("Version",       [] { return uint32_t(3); });
        m_object.addProperty("TextDirection", [] { return std::string("ltr"); });
        m_object.addProperty("Status",        [] { return std::string("normal"); });
        m_object.addProperty("IconThemePath", [] { return std::vector<std::string>(); });

        m_object.addMethod("GetLayout", "iias", "u(ia{sv}av)", [this](MethodCall& call) {
            int32_t parent = 0, depth = -1;
            std::vector<std::string> names;
            call.read(parent, depth, names);
            call.reply(m_revision, layout(parent, depth));
        });
        m_object.addMethod("GetGroupProperties", "aias", "a(ia{sv})", [this](MethodCall& call) {
            std::vector<int32_t> ids;
            std::vector<std::string> names;
            call.read(ids, names);
            std::vector<std::tuple<int32_t, VariantMap>> result;
            for (int32_t id : ids)
                if (const Entry* e = find(id))
                    result.emplace_back(id, props(*e));
            call.reply(result);
        });
        m_object.addMethod("GetProperty", "is", "v", [this](MethodCall& call) {
            int32_t id = 0; std::string name;
            call.read(id, name);
            const Entry* e = find(id);
            const auto map = e ? props(*e) : VariantMap{};
            const auto it  = map.find(name);
            call.reply(it != map.end() ? it->second : Variant(std::string()));
        });
        m_object.addMethod("Event", "isvu", "", [this](MethodCall& call) {
            int32_t id = 0; std::string event;
            call.read(id, event);
            call.reply();
            if (event != "clicked") return;
            const Entry* e = find(id);
            if (!e || !e->action || !e->enabled) return;
            Lingmo::base::EventLoop::current()->post(
                [action = e->action, alive = m_alive] { if (*alive) action(); });
        });
        m_object.addMethod("AboutToShow", "i", "b", [this](MethodCall& call) {
            int32_t id = 0;
            call.read(id);
            update();
            call.reply(true);
        });
        m_object.addSignal("LayoutUpdated",          "ui");
        m_object.addSignal("ItemsPropertiesUpdated",  "a(ia{sv})a(ias)");
        m_object.addSignal("ItemActivationRequested", "iu");
        m_object.publish();

        // Re-register whenever the registrar restarts.
        m_watch = bus->watchName(kRegistrar, [this, alive = m_alive](const std::string& owner) {
            if (*alive && !owner.empty())
                registerWithBar();
        });
    }

    ~GlobalMenuExporter()
    {
        *m_alive = false;
        Lingmo::base::dbus::Proxy reg(m_bus, kRegistrar, kRegistrarPath, kRegistrar);
        reg.send("UnregisterWindow", uint32_t(kWindowId));
    }

    // Call after changing the menu entries (e.g. mode switch).
    void update()
    {
        rebuild();
        ++m_revision;
        m_object.emitSignal("LayoutUpdated", m_revision, int32_t(0));
        for (const auto& [id, _] : m_ids)
            m_object.emitSignal("LayoutUpdated", m_revision, id);
    }

private:
    static constexpr const char* kInterface    = "com.canonical.dbusmenu";
    static constexpr const char* kRegistrar    = "com.canonical.AppMenu.Registrar";
    static constexpr const char* kRegistrarPath= "/com/canonical/AppMenu/Registrar";
    static constexpr const char* kMenuPath     = "/org/lingmo/Calculator/Menu";
    static constexpr uint32_t    kWindowId     = 1;  // non-zero = regular app window

    using VariantMap = Lingmo::base::dbus::VariantMap;
    using Variant    = Lingmo::base::dbus::Variant;
    using MethodCall = Lingmo::base::dbus::MethodCall;
    using Layout     = std::tuple<int32_t, VariantMap, std::vector<Variant>>;

    void registerWithBar()
    {
        Lingmo::base::dbus::Proxy reg(m_bus, kRegistrar, kRegistrarPath, kRegistrar);
        reg.send("RegisterWindow", kWindowId,
                 Lingmo::base::dbus::ObjectPath{kMenuPath});
    }

    void rebuild()
    {
        m_entries = m_menu();
        m_ids.clear();
        m_entryIds.clear();
        int32_t next = 1;
        std::function<void(const std::vector<Entry>&)> number =
            [&](const std::vector<Entry>& entries) {
                for (const Entry& e : entries) {
                    m_ids[next] = &e;
                    m_entryIds[&e] = next++;
                    number(e.children);
                }
            };
        number(m_entries);
    }

    const Entry* find(int32_t id) const
    {
        const auto it = m_ids.find(id);
        return it == m_ids.end() ? nullptr : it->second;
    }

    VariantMap props(const Entry& e) const
    {
        VariantMap p;
        if (e.separator) {
            p["type"] = Variant(std::string("separator"));
        } else {
            p["label"]         = Variant(e.label);
            p["toggle-type"]   = Variant(std::string(e.checked ? "checkmark" : ""));
            p["toggle-state"]  = Variant(int32_t(e.checked ? 1 : 0));
        }
        if (!e.enabled)              p["enabled"] = Variant(false);
        if (!e.children.empty())     p["children-display"] = Variant(std::string("submenu"));
        return p;
    }

    Layout layout(int32_t id, int depth) const
    {
        VariantMap itemProps;
        if (id == 0) {
            itemProps["children-display"] = Variant(std::string("submenu"));
        } else if (const Entry* e = find(id)) {
            itemProps = props(*e);
        }
        std::vector<Variant> items;
        const std::vector<Entry>& children = (id == 0) ? m_entries
            : (find(id) ? find(id)->children : m_entries);
        if (depth != 0) {
            for (const Entry& child : children) {
                const auto it = m_entryIds.find(&child);
                if (it == m_entryIds.end()) continue;
                items.push_back(Variant(layout(it->second, depth - 1)));
            }
        }
        return {id, itemProps, items};
    }

    Lingmo::base::dbus::Connection*       m_bus;
    std::function<std::vector<Entry>()>   m_menu;
    Lingmo::base::dbus::Object            m_object;
    Lingmo::base::dbus::NameWatch         m_watch;
    std::vector<Entry>                    m_entries;
    std::map<int32_t, const Entry*>       m_ids;
    std::map<const Entry*, int32_t>       m_entryIds;
    uint32_t                              m_revision = 1;
    std::shared_ptr<bool>                 m_alive = std::make_shared<bool>(true);
};

// ─── CalculatorWindow ─────────────────────────────────────────────────────────

const char* CalculatorWindow::modeName(Mode mode)
{
    switch (mode) {
    case Mode::Standard:   return "Standard";
    case Mode::Scientific: return "Scientific";
    case Mode::Programmer: return "Programmer";
    case Mode::Date:       return "Date Calculator";
    case Mode::Graph:      return "Graphing";
    default:               return "Standard";
    }
}

CalculatorWindow::CalculatorWindow()
{
    m_window.setTitle(tr("Calculator"));
    m_window.setMinimumSize({300, 400});

    // Window: plain header (title + close/min/max) + content area.
    auto root = std::make_unique<CalcRoot>();
    m_root = root.get();

    auto* titleLabel = m_root->header()->setContent<Label>(tr("Calculator"));
    titleLabel->setFontSize(14.5);
    titleLabel->setWeight(600);

    View* content = m_root->content();
    m_standardPage   = content->addChild<StandardPage>();
    m_scientificPage = content->addChild<ScientificPage>();
    m_programmerPage = content->addChild<ProgrammerPage>();
    m_datePage       = content->addChild<DatePage>();
    m_graphPage      = content->addChild<GraphPage>();

    m_window.setContentView(std::move(root));

    // Register the mode menu with the compositor's global menu bar.
    auto* bus = Lingmo::base::dbus::Connection::session();
    if (bus) {
        m_menuExporter = std::make_unique<GlobalMenuExporter>(bus, [this] {
            using Entry = GlobalMenuExporter::Entry;
            struct ModeItem { Mode mode; const char* label; };
            static constexpr ModeItem kModes[] = {
                { Mode::Standard,   "Standard"        },
                { Mode::Scientific, "Scientific"      },
                { Mode::Programmer, "Programmer"      },
                { Mode::Date,       "Date Calculator" },
                { Mode::Graph,      "Graphing"        },
            };
            Entry modeMenu;
            modeMenu.label = tr("Mode");
            for (const auto& mk : kModes) {
                Entry item;
                item.label   = tr(mk.label);
                item.checked = (m_mode == mk.mode);
                item.action  = [this, mode = mk.mode] { setMode(mode); };
                modeMenu.children.push_back(std::move(item));
            }
            return { std::move(modeMenu) };
        });
    }

    // Restore saved mode.
    Lingmo::base::Settings settings(Lingmo::base::paths::lingmoConfig("calculator"));
    const int saved = int(settings.intValue("window/mode", 0));
    const Mode initial = (saved >= 0 && saved < int(Mode::Count))
                         ? static_cast<Mode>(saved) : Mode::Standard;
    applyMode(initial);
}

CalculatorWindow::~CalculatorWindow()
{
    Lingmo::base::Settings settings(Lingmo::base::paths::lingmoConfig("calculator"));
    settings.setValue("window/mode", int64_t(m_mode));
    settings.sync();
}


void CalculatorWindow::applyMode(Mode mode)
{
    m_mode = mode;
    const int idx = int(mode);

    m_standardPage  ->setVisible(mode == Mode::Standard);
    m_scientificPage->setVisible(mode == Mode::Scientific);
    m_programmerPage->setVisible(mode == Mode::Programmer);
    m_datePage      ->setVisible(mode == Mode::Date);
    m_graphPage     ->setVisible(mode == Mode::Graph);

    m_window.resize({kModeWidths[idx], kModeHeights[idx]});

    // Push the updated checkmarks to the status bar menu.
    if (m_menuExporter)
        m_menuExporter->update();
}

void CalculatorWindow::setMode(Mode mode)
{
    if (m_mode == mode) return;
    applyMode(mode);
}

// Standard-mode delegation (preserves test API).
void CalculatorWindow::append(const std::string& text)     { m_standardPage->append(text); }
void CalculatorWindow::backspace()                          { m_standardPage->backspace(); }
void CalculatorWindow::clear()                              { m_standardPage->clear(); }
void CalculatorWindow::clearHistory()                       { m_standardPage->clearHistory(); }
void CalculatorWindow::evaluate()                           { m_standardPage->evaluate(); }
void CalculatorWindow::copyResult()                         { m_standardPage->copyResult(); }
const std::string& CalculatorWindow::expression() const     { return m_standardPage->expression(); }
std::string CalculatorWindow::result() const
{
    const std::string& r = m_standardPage->result();
    return r.empty() ? std::string("0") : r;
}
const std::vector<std::string>& CalculatorWindow::history() const { return m_standardPage->history(); }
void CalculatorWindow::focus() { m_standardPage->focus(); }

} // namespace calculator
