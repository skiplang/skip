## How to run it?

With the JavaScript backend:

```js
./run.sh file.sk
```

With the native backend:

```js
cd ../../../build
ninja skip_printer
./bin/skip_printer file.sk
```

## How to test it?

This will run the printer twice on every single valid sk file ever written so far and make sure that pp(pp(file)) == pp(file). This was the test that found most bugs in prettier.

```js
./test.sh
```
