# QtSpimbot Enviornment Setup Instructions

## Normal Setup

```
sudo apt install git libqt5gui5 bison flex qt5-default libboost-random-dev
```

or if you are not on a LTS version of Ubuntu:

```
sudo apt install git libqt5gui5 bison flex qtbase5-dev qtchooser qt5-qmake qtbase5-dev-tools libboost-random-dev
```

git clone https://github.com/cs233/qtspimbot.git

Make the program with `qmake && make -j<number of threads on your machine>` while in `/QtSpim`

You can debug the program with `qmake DEBUG=1 && make -j<number of threads on your machine>` which will add debug symmbols that can be read by gdb.

## WebAssembly Setup

```
sudo apt install git bison flex     # linux
brew install flex                   # mac
```



```
git clone https://github.com/cs233/qtspimbot.git
```

### Install Emscripten
```
git clone https://github.com/emscripten-core/emsdk.git
```

```
# mac-only
./emsdk install binaryen-main-64bit
./emsdk activate binaryen-main-64bit

# mac+linux
./emsdk install 1.39.8
./emsdk activate 1.39.8
source ./emsdk_env.sh
```

You will need to run `source ./emsdk_env.sh` everytime you load a new terminal, so it's good to add it to your `.bashrc`  or `.bash_profile`.

### Installing Boost

Boost normally installs within the g++ libraries. However, since we are using em++, it uses its own libraries.

So, we need to paste in the boost headers into emscripten.

```
wget https://github.com/emscripten-ports/boost/releases/download/boost-1.75.0/boost-headers-1.75.0.zip
unzip boost-headers-1.75.0.zip -d <path to emsdk>/upstream/emscripten/system/include
```

### Install QT 5

We are going to be using Qt 5.15.2 since its corresponding known-good version of Enscripten is 1.39.8

```
wget https://download.qt.io/archive/qt/5.15/5.15.2/single/qt-everywhere-src-5.15.2.tar.xz
tar -xf qt-everywhere-src-5.15.2.tar.xz
mv qt-everywhere-src-5.15.2 qt-5.15.2
```

```
cd qt-5.15.2
mkdir build && cd $_
../configure -static -release -no-pch -xplatform wasm-emscripten -skip qtwebengine -nomake tools -nomake tests -nomake examples
```

If you are having trouble, try adding `-no-feature-thread` to the last command to enable single threaded configuration wasm.

On linux, the last thing that is output suggests you to install libclang-dev which can be done with:

```
sudo apt install libclang-dev
```

Now, Build Qt5:

```
time make -j6
```

the following should work as well

```
time make -j<as many threads as possible bc this will take a LONG time> install
```

For my i5-8500 (6 threads), it took a whopping 44 mins 39.9 secs! 

qmake will then be installed in build/qtbase/bin

```
sudo ln -s $PWD/qtbase/bin/qmake /usr/bin/qmake-wasm <--- causing errs bruhhhhh

# try this one 
(printf 'alias qmake-wasm=\"%s/qtbase/bin/qmake\"\n' $PWD) >> ~/.bash_profile; source ~/.bash_profile
```

Now, when you do `qmake-wasm --version`, the output should be:

```
QMake version 3.1
Using Qt version 5.15.2 in
```

### Running JsSpim

```
python3 emrun.py --no_browser index.html
```

then open `localhost:6931` in your browser
