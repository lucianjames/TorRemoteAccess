# Remote access shell protocol
## Initial connection from client to server
The client will connect to the server, adding itself to the server's list of clients. The client will then send a message to the server, containing the following information:
* The client's IP address
* The client's system username
* The client's system hostname
The format for this message is as follows:
```
<ip>;<username>;<hostname>;
```
The server will then respond with a message that acknowledges the client's connection, letting the client know it can wait for commands. This message will contain a copy of the information the client sent, as well as the word "connected" at the end of the message. The format for this message is as follows:
```
<ip>;<username>;<hostname>;connected;
```
Once the client receives this message, it will simply wait for commands from the server.

## Commands
### Required commands
The following commands must be implemented by the client:
* `cd <path>` - Change the current working directory to the specified path
* `ls` - List the contents of the current working directory
* `pwd` - Print the current working directory
* `grab <remotepath>` - Sends the contents of the file at the specified path to the server (raw bytes)
* `upload <localpath> <remotepath>` - Uploads a file to the specified path on the client (raw bytes)
* `exec <command>` - Executes the specified command on the client (via `system()`)
* `ping` - Pings the client, returns "pong" if the client is still connected
### Potential commands
The following commands may be considered:
* `ensurestartup` - Ensures that the client is started on boot via registry keys, antivirus will likely flag this
* `uninstall` - Uninstalls the client from the system
* `shutdown` - Shuts down the client machine
* `reboot` - Reboots the client machine
### Evil commands (not recommended for legal reasons)
* `nuke` - Computer annihilator ;) - this command must be given the argument "iamreallysureaboutthis" to execute
* `grabtokens` - Grabs common tokens from the system (discord, steam, etc.)

## Commands - detailed specification
### `cd <path>`
This command will change the current working directory to the specified path.
The format of this command from the server to the client is as follows:
```
cd;<path>;
```
The client responds with the following message (upon success):
```
cd;<path>;success;
```
Failure will send a generic failure message
### `ls`
This command will list the contents of the current working directory.
The format of this command from the server to the client is as follows:
```
ls;
```
The client responds with the following message (upon success):
```
ls;<string containing directory contents>;
```
Failure will send a generic failure message
### `pwd`
This command will print the current working directory.
The format of this command from the server to the client is as follows:
```
pwd;
```
The client responds with the following message (upon success):
```
pwd;<path>;
```
Failure will send a generic failure message
### `grab <remotepath>`
This command will send the contents of the file at the specified path to the server as raw bytes.
The format of this command from the server to the client is as follows:
```
grab;<remotepath>;
```
The client responds with the following message (upon success):
```
grab;<remotepath>;<raw bytes>;
```
The server will then handle the raw bytes as it sees fit.
Failure will send a generic failure message
### `upload <localpath> <remotepath>`
This command will upload a file from the specified local path to the specified remote path on the client.
The format of this command from the server to the client is as follows:
```
upload;<remotepath>;<raw bytes>;
```
The client responds with the following message (upon success):
```
upload;<remotepath>;success;
```
Failure will send a generic failure message
### `exec <command>`
This command will execute the specified command on the client via `system()`.
The format of this command from the server to the client is as follows:
```
exec;<command>;
```
The client responds with the following message (upon success):
```
exec;<command>;<output>;
```
Failure will send a generic failure message
### `ping`
This command will ping the client, returning "pong" if the client is still connected.
The format of this command from the server to the client is as follows:
```
ping;
```
The client responds with the following message (upon success):
```
ping;pong;
```
Failure will send a generic failure message
### Other commands
The other commands are not specified here, as they are not required to be implemented by the client. They are simply listed here for reference and potential future use.