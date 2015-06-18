
## configuring the battery charger

`input_current <mA>` - set the maximum USB input current.
can be set from 100 to 2100 millamps. you should not set this
higher than 500 millamps unless you are supplying
power via a non-USB source via the Vin pin.

`charge_current <mA>` - set the battery charging current.
can be set from 50 to 1300 milliamps. set this to the value
recommended by your battery manufacturer, setting a higher value
may damage the battery.

`charge_status` - show the current charging status


## example
```
input_current 500
charge_current 200
```


