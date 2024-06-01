# Network Programming Project 4 - SOCKS 4

Implement the **SOCKS 4/4A protocol** in the application layer of the OSI model.

*Use **Boost.Asio** library to accomplish this project.*

## Compile

### Build

```
make
```

### Execution

Run the **SOCKS server**

```
./socks_server [port]
```

## Testing

### Part I: SOCKS 4 Server `Connect` Operation

- Turn on and set your SOCKS server, then
    - Be able to connect any webpages on Google Search.
    - Only test this section on ***Firefox***. (Because it can manually set the proxy)
    - SOCKS server will show messages below:
      
      ```
      <S_IP>: source ip
      <S_PORT>: source port
      <D_IP>: destination ip
      <D_PORT>: destination port
      <Command>: CONNECT or BIND
      <Reply>: Accept or Reject
      ```

### Part II: SOCKS 4 Server `Bind` Operation

- Connect to FTP server with SOCKS
  
    - Set config with your SOCKS server
    - Connection type is **FTP** (cannot be SFTP)
    - Data connection mode is **Active Mode (PORT)**
      
        - Can use ***FlashFXP*** as the FTP client
        
- Download files larger than 1GB completely. E.g., Ubuntu 20.04 ISO image ([download link](http://free.nchc.org.tw/ubuntu-cd/focal/ubuntu-20.04.1-desktop-amd64.iso))
  
    - The SOCKS server’s output message should show that BIND operation is used

### Part III: CGI Proxy

- Clear browser’s proxy setting
  
- Open your http server, connect to **panel_socks.cgi**

- Key in IP, port, filename, SocksIP, SocksPort

- Connect to ras/rwg servers through SOCKS server and check the output Test Case (same as Project 3)

### Firewall

-  List permitted **destination** IPs into `socks.conf` (deny all traffic by default)

    ```
    e.g.,
    permit c 140.113.*.*  # permit NYCU IP for Connect operation
    permit b *.*.*.*      # permit all IP for Bind operation
    ```
