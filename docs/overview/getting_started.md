---
id: getting_started
title: Getting Started
---

There are two main options for compiling and running Skip programs. To quickly experiment with Skip we recommend using the [Docker container](#docker-image). For a full development environment and/or to develop Skip itself, see the [guide](#developing-skip). If you want to develop Skip using a docker image, you can also do that, verlaguet/skip:dev is a good image to get started, it has all the necessary packages installed.

## Docker Image

A docker image is available to download to quickly experiment with running Skip programs without installing additional software (beyond Docker itself). To use the image:

1. Install [Docker](https://www.docker.com/).

2. Create a new directory for writing your Skip programs:

```
# In your favorite directory:
mkdir skip
```

Note: for the rest of this guide, `<skip-dir>` refers to the path you created in this step.

3. Run the Skip docker image (it will be downloaded the first time and then cached):

```
# Replace <skip-dir> with the directory you created above
docker run -it --user skip --mount type=bind,source=<skip-dir>,target=/home/skip/app verlaguet/skip
```

When this step completes you should now have a command prompt in the docker container.

4. Install the Skip toolchain within the docker container:

```
# From docker container shell
> cd /home/skip
> ./install.sh
```

This will install tools necessary to build Skip all within the docker container's temporary file system.

5. Run the example app (see the [readme](https://github.com/skiplang/skip/blob/master/apps/bundler/README.md)) for more information about the example app.

```
# From docker container shell
> cd /home/skip/skip/apps/bundler
> ./run.sh --root example
```

6. You can also write, compile and run Skip programs:

a. Create a Skip file at `<skip-dir>/example.sk`, for example:

```
// <skip-dir>/example.sk
fun main(): void {
  print_string("hello world!")
}
```

b. Run it with the `run.sh` helper:

```
# From docker container shell
> cd /home/skip/apps
> ls # should show example.sk created above
> ../skip/run.sh example.sk
```

### Abbreviated Version

A full example session using the docker image:

```
# your shell
cd /tmp
mkdir skip
docker run -it --user skip --mount type=bind,source=skip,target=/home/skip/app verlaguet/skip
# now in the docker shell
cd /home/skip
./install.sh
cd skip/apps/bundler
./run.sh --root example
```

### Notes

The docker container uses a temporary file system by default, which means that the Skip toolchain installed with `install.sh` is not persisted when re-running the container. If you use the container frequently, you may want to configure docker to use a persisted storage location for the container's file system (such as a folder on your main operating system's file system).

## Developing Skip

To develop the Skip language and toolchain itself, [follow the guide](https://github.com/skiplang/skip/blob/master/docs/developer/README-cmake.md).
