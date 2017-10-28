# picam2fits

This is a simple command line tool to convert, without loss of information, shots taken with the Raspberry Pi Camera v2 into FITS images. The shots must be taken with the ```--raw``` (```-r```) option of ```raspistill```:
```bash
raspistill -r -o image.jpg
picam2fits image.jpg
```

This will generate the file ```image.fits```, which contains four FITS extensions (plus the primary HDU, which is empty), one for each color channel: red, green, green (again), and blue. There are, indeed, two green channels because of the Bayer pattern of the camera's detector:
```
+--+--+--+--+--+
|BG|BG|BG|BG|BG|
|GR|GR|GR|GR|GR|
+--+--+--+--+--+
|BG|BG|BG|BG|BG|
|GR|GR|GR|GR|GR|
+--+--+--+--+--+
|BG|BG|BG|BG|BG|
|GR|GR|GR|GR|GR|
+--+--+--+--+--+
```

The image is shifted by half a pixel in the X and Y directions between the two green channels, and thus they provide independent information about the image. This is reflected in the WCS coordinate system, which provides the position of each pixel on the detector (in microns, from the bottom-left corner).

The output images have 1640x1232 pixels per channel, with integer values from 0 to 1023. The program will also read the JPG EXIF data to save the exposure time and date in the FITS header.


# Install instructions

```bash
sudo apt-get install cmake libcfitsio-dev libexif-dev
git clone https://github.com/cschreib/picam2fits.git
cd picam2fits
mkdir build
cd build
cmake ../
make
sudo make install
```


# Recommended camera settings and workflow

If you are interested in raw data and FITS files, you probably are working on a project that requires stable, repeatable observations. This should include first and foremost a stable camera configuration, without any of the automatic tuning that is used for casual photography. This can be achieved with the following:
```bash
ANALOG_GAIN=1.0   # >=1.0 and <=8.0
DIGITAL_GAIN=1.0  # >=1.0
EXPTIME=50000     # us

raspistill -t 1 -ex auto -ag ${ANALOG_GAIN} -dg ${DIGITAL_GAIN} \
    -awb off -drc off -ss ${EXPTIME} -r -o image.jpg
```

This setup offers a fixed exposure time ```EXPTIME``` (expressed in microseconds), no white balance correction or dynamic range compression, and a fixed ISO sensitivity through the choice of the analog and digital gains (this requires a recent version of ```raspistill```, from October 2017 onwards). You may tweak ```ANALOG_GAIN``` for your observed scene, so as to make best use of the camera's 10 bit dynamic range and avoid saturation. ```DIGITAL_GAIN``` should be left equal to one since it provides no advantage over the analog gain, and using it to go beyong the maximum analog gain of 8 only makes sense when dealing with 8 bit data.

The option ```-t 1``` disables the default 5 second delay before the image is actually captured. For even faster response, use the options ```-t 0 -s -fs 1 -v -o image_%d.jpg```. This will leave ```raspistill``` to run continuously, with the camera ready to shoot. You can then trigger a capture at any time by sending the ```SIGUSR1``` (10) signal to the program. To do so, simply run ```kill -s 10 $(pgrep raspistill)```. You can run this as many times as you wish, then take a last shot and terminate gracefully with ```kill -s 12 $(pgrep raspistill)```. The shots will be named ```image_1.jpg```, ```image_2.jpg```, and so on and so forth. You can finally convert them all to FITS files using ```picam2fits image_*.jpg```.
