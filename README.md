### Build

Sprites live in onboard memory and thus need to be precompiled before flashing onto the pico.
To convert .bmp files in the sprites directory run:
```bash
./convert_sprites.sh
```

Run the following command:

```bash
rm -rf build && mkdir build && cd build
cmake .. -DPICO_SDK_PATH=$PICO_SDK_PATH
make -j4
cd ..
```

or just run the build script (just has the above commands):

```bash
./build.sh
```