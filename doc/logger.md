

## features

* depending on the config, logging 2 analog channels at audio rates is possible
* can programmatically control whether to log or not log
* wide variety of file formats
* log analog or digital values

## configuring the data logger

`logger_logfile = <filename>` - set the output file name

`logger_values <list of pins>` - specify which pins to log

`logger_format <format>` - specify the data file format. currently supported:
* csv   - comma separated values
* json  - newline delimited json
* xml   - record oriented, partial xml (missing outer wrapper)
* raw   - raw 16bit little-endian values
* sln   - raw signed-linear 16bit little-endian values
* alaw  - 8 bit a-law compressed
* xlaw  - 8 bit unsigned 3e/5m compressed
* bits  - raw 1 bit per value, for digital inputs
* tca   - a-law with timestamps + silence suppression

`logger_rate <rate>` - specify the logging sampling rate.
depending on your config, you may be able to go from
usecs to years.
suffix with `u` for microseconds, `m` for milliseconds,
`M` for minutes, `H` for hours, `D` for days.


`logger_lowpower = <0,1>` if set to 1, system will enter low-power standby mode
at slow sampling rates

`logger_script = <filename>` - specify a script to run at each sampling. script
can choose to log or not log the samples, or perform other actions.

`logger_autostart = <0,1>` - start the logger automaitically at boot

`logger_start` - manually start the logger
`logger_stop`  - stop the logger
`logger_reload` - reload logging parameters
`logger_rotate` - rotate the log file


## example
```
pinmode A0 adc
pinmode A1 adc
logger_values A0 A1
logger_rate 1m
logger_logfile = data.log
logger_format xml
logger_start
```


