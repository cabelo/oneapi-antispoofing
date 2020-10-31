# oneapi-antispoofing

***Certiface AntiSpoofing*** use oneAPI for fast decode video for perform liveness detection with inference. The system is capable of spotting fake faces and performing anti-face spoofing in face recognition systems.  The user could try to hold up a your photo. Maybe they even have a photo or video on their smartphone (obtained in facebook, for example)

Technologies Used:
    oneAPI
    openCV
    openVINO

## Installation instructions:

***Under construction***

Below, a quick description of how to manually install the VisionPaste project.

### Clone the project

Clone the repository at desired location:

``` bash
$ git clone https://github.com/cabelo/oneapi-antispoofing
$ cd oneapi-antispoofing
$ mkdir build
$ cd build
$ cmake ..
 * Serving Flask app "server" (lazy loading)
 * Environment: production
   WARNING: Do not use the development server in a production environment.
   Use a production WSGI server instead.
 * Debug mode: off
 * Running on https://192.168.0.4:8080/ (Press CTRL+C to quit)


```


### To Do
- [x] First version
- [ ] Publish in github
- [ ] Port to oneAPI Beta10
- [ ] Create webserver REST

## The final result

Bellow an example running in my machine with OpenSUSE Leap 15.2.

contact : Alessandro de Oliveira Faria (A.K.A.CABELO) cabelo@opensuse.org

![](img/fraud.gif)

![](img/ok.gif)

