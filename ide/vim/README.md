To use this very minimal syntax highlighting run

```
mkdir -p ~/.vim/ftdetect
cp ftdetect/skip.vim ~/.vim/ftdetect
mkdir -p ~/.vim/syntax
cp syntax/skip.vim ~/.vim/syntax
```

Alternatively, if you routinely keep a copy of the skip source tree, add code
similar to the following to your `.vimrc` so that vim can directly use the
source.

```
set runtimepath+=~/skip/ide/vim
```
