# ncc
A tiny C compiler written in C.

## development
```sh
$ git clone git@github.com:natsuokawai/ncc.git
$ cd ncc
$ docker build -t ncc .
$ docker run --rm -it -v ~/path/to/ncc:/ncc -w /ncc ncc
```

## reference
- https://www.sigbus.info/compilerbook
- https://github.com/rui314/chibicc
