# Commands

## Generic commands
Generic commands that dont require special processing can be sent to the client using the following format:

`<cmd>;<argument>;`

The response from the client will then be:

`<cmd>;<response>;`

Currently, only the `cd` command uses this.

## Exec
Sends a command to the client to then be executed via _popen(), sent using the following format:

`exec;<command>;`

The client responds with:

`exec;<command>;<response size>;<response>;`

Thanks to the fact that the response size is sent to the server, there is no limit on how much data exec can return.
However, the current implementation of the client uses an std::string for the response, so null bytes in the buffer will cause problems!

## Grab
Transfers a file from the client to the server, the server sends the following request to the client:

`grab;<file path>;`

The client will then respond with the following:

`grab;<file path>;<file size>;<file data>;`

On fail, the client will set the file size to 0 and the file data to ERR

## Upload
Grab but in reverse, allows the server to place a file onto the client machine.

`upload;<file name>;<file size>;<file data>;`

Client response:

`upload;<failed/success>`

