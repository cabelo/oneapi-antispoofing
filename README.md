# oneapi-antispoofing

***Certiface AntiSpoofing*** use oneAPI for fast decode video for perform liveness detection with inference. The system is capable of spotting fake faces and performing anti-face spoofing in face recognition systems.  The user could try to hold up a your photo. Maybe they even have a photo or video on their smartphone (obtained in facebook, for example)

Technologies Used:
    oneAPI
    openCV
    openVINO

## Installation instructions:

***Under construction***

Below, a quick description of how to manually install the VisionPaste project.

### Prepare ambient

Install the openCV library and set up the oneapi development environment as in the example below.

``` bash
$ source /opt/intel/oneapi/setvars.sh 
:: initializing oneAPI environment ...
:: dnnl -- latest
:: advisor -- latest
:: dpcpp-ct -- latest
:: dev-utilities -- latest
:: ipp -- latest
:: ccl -- latest
:: compiler -- latest
:: ippcp -- latest
:: daal -- latest
:: debugger -- latest
:: mpi -- latest
:: intelpython -- latest
:: tbb -- latest
:: vpl -- latest
:: vtune -- latest
:: mkl -- latest
:: oneAPI environment initialized ::

```

### Clone and build the project

Clone the repository at desired location:

``` bash
$ git clone https://github.com/cabelo/oneapi-antispoofing
$ cd oneapi-antispoofing
$ mkdir build
$ cd build
$ cmake ..
$ make

```

### Run the example

Below, how to run the project. Enter in build folder and execute the commands:

``` bash
$ wget https://service.assuntonerd.com.br/downloads/antispoofing.weights
$ wget https://service.assuntonerd.com.br/downloads/antispoofing.cfg
$ make run

```

### To Do
- [x] First version
- [x] Publish in github
- [ ] Port to oneAPI Beta10
- [ ] Create webserver REST

## The final result

Bellow an example running in my machine with OpenSUSE Leap 15.2.

contact : Alessandro de Oliveira Faria (A.K.A.CABELO) cabelo@opensuse.org

![](img/fraud.gif)

![](img/ok.gif)

