# lingmo-calculator

A native Lingmo OS calculator with exact decimal arithmetic.

## Features

- `+`, `-`, `*`, `/`, `^`, parentheses and percentages
- Exact decimals through the HMath engine
- Keyboard input and a history of recent calculations
- Light and dark themes

## Build

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/lingmo-sdk
cmake --build build
sudo cmake --install build
```

The `engine/` directory keeps the SpeedCrunch HMath number engine under its
original GPL-2.0-or-later notices; the expression reader and the interface are
new.
