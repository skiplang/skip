How to run the website:

```
yarn
yarn start
```

If you want to run both the website and the playground:

```
cd ../build
cmake ..
ninja skip_to_native skip_to_js skip_printer skip_to_ast skip_to_parsetree
cd ../website
yarn
cd playground
yarn
yarn build
cd ..
./run-all.sh
```

For the above to work, please use a node version >4.8.0.
Try /usr/bin/node/ or /bin/node.

To extract standard lib documentation:

```
./extract-docs.sh
```
