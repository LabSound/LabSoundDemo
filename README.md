# LabSoundDemo

clone this repository recursively

```sh
git clone https://github.com/LabSound/LabSoundDemo
```

## LabSoundDemo

Build and install. The install is necessary to put the sample audio files in the right place.

```sh
mkdir build
cd build
cmake -- -DCMAKE_INSTALL_PREFIX="./install"
cmake --build . --target install --config Release
```

Run the demo

```sh
./install/bin/LabSoundDemo
```

### Note for IDE users

After running the cmake configuration step with the generator set to your IDE, open the resulting IDE file, and build the INSTALL target.

Set your run target to LabSoundDemo, and you will be able to run it in the IDE's debugger subsequently.

## LabSoundStarter

LabSoundStarter is a minimal Hello World example. Use it as a jumping off point for experimentation!

It is built and install via the steps detailed for LabSoundDemo.

