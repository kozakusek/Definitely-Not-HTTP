# "À la HTTP/1.1" server

This an implementation of a subspecification of HTTP/1.1 server
made as one of the tasks for 'Computer Networks' @MIMUW.

## Usage

```
serwer <path-to-directory-with-files> <path-to-file-with-correlated-servers> [<server-port>]
```

## Short description

#### The server accepts only two of HTTP methods:
 * __GET__: Server tries to locate the file ```path-to-directory-with-files/request-target```. If it manages to do so, it sends a response with the file's content in the _body_ section. Otherwise it checks whether any correlated servers from ```path-to-file-with-correlated-servers``` has ```request-target```, if yes it sends the address of any server with ```reuqest-target```, 
 however if not, the server responds with error 404.

 * __HEAD__: Same as __GET__ but without body  

#### The only possible status codes are:

__200__: completely correct  
__302__: resource found at one of the correlated servers  
__400__: incorrect format  
__404__: resource not found  
__500__: server side error occured  
__501__: request method beyond the scope of implementation  

