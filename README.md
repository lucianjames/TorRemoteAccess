# TorRemoteAccess
!! VERY WORK IN PROGRESS !!

![image](https://user-images.githubusercontent.com/65134690/212556242-96c63b4e-0faa-4404-8ef9-35898eb66ac3.png)

# Setup
## Compiling the server
```
git clone --recurse-submodules https://github.com/LJ3D/TorRemoteAccess
cd TorRemoteAccess/TRAServer
mkdir build
cd build
cmake ..
cmake --build .
```

## Compiling the client
!!! You should probably replace tor.exe with your own file from the tor expert bundle, which you can download from https://www.torproject.org/download/tor/

There is a tor executable included in the repository, but it may be out of date (and you may not want to trust some random .exe from some random repo :) )

Once youve replaced the tor.exe file in `TRAClient/TRAClient/resources` all you need to do is open the sln file and click the build button.

# How to use
## Creating a TOR hidden service
See https://community.torproject.org/onion-services/setup/

Currently, the server listens on port 52727 for connections from the proxy, and the client connects to the hidden service at port 1337, so update your torrc accordingly!

## Running the server
This program is designed in such a way that you dont need to run the server 24/7 at all, clients will attempt to connect over and over again until a successful connection is made. This means all you have to do is run the server program when you feel like taking control!

## Configuring the client
Swap out the onion address/port with your own, and youre good to go!

You will likely want to disable the console window and debug prints from torplusplus, doing so is just a case of changing a few "#define"s.

# Educational purposes!
The code in this repository is not at all intended for real-world use as a malicious remote access tool, there are a variety of flaws in the way it operates which would make reverse engineering the program far too easy.

Additionally, this repository contains no code that would assist you in delivering a payload, or ensuring persistence.

No skids allowed here, you will have to make modifications and additions yourself :)
