# Commands
## pwd
Sends as:
`pwd;`
Response:
`pwd;<current directory>;`

## ls
Sends as:
`ls;`
Response:
`ls;<file/folder/>;<file/folder/>;...;`

## rm
Sends as:
`rm;<path>;`
Response:
`rm;<path>;success;` OR `rm;<path>;failed;`

## cd
Sends as:
`cd;dir;`
Response:
`cd;success;` OR `cd;failed;`

## grab
Sends as:
`grab;<path>;`
Response:
`grab;<path>;<filesize>;<raw file data>;`

## upload
Sends as:
`upload;<filename>;<filesize>;<raw file data>;`
Response:
`upload;<filename>;success;` OR `upload;<filename>;failed;`

## exec
Sends as:
`exec;<command>;`
Response:
`exec;<command>;<output>;`