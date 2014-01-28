## MPC -- A asynchronous HTTP benchmark tool

## Usage

```shell

Usage: mpc [-hvfr] [-l log file] [-L log level] 
           [-c concurrency] [-u url file] [-m http method]
           [-R result file] [-M result mark string] 
           [-a specified address] [-t run time]

Options:
  -h, --help            : this help
  -v, --version         : show version and exit
  -C, --conf=S          : configuration file
  -u, --url-file        : url file
  -a, --address=S       : use address specified instead of DNS
  -f, --follow-location : follow 302 redirect
  -r, --replay          : replay the url file
  -l, --log-file=S      : log file
  -L, --log-level=S     : log level
  -c, --concurrency=N   : concurrency
  -m, --http-method=S   : http method GET, HEAD
  -R, --result-file=S   : show result in a file
  -M, --result-mark=S   : result file mark string
  -t, --run-time=Nm     : timed testing where "m" is modifer
                          S(second), M(minute), H(hour), D(day)

```

## Author

FengGu, <flygoast@126.com>
