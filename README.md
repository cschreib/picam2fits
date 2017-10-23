# picam2fits

This is a simple command line tool to convert, without loss of information, shots taken with the Raspberry Pi Camera v2 into FITS images. The shots must be taken with the ```--raw``` option of ```raspistill```:
```bash
raspistill --raw -o image.jpg
picam2fits image.jpg image.fits
```

The generated FITS file contains four extensions (plus the primary HDU, which is empty), one for each color channel: red, green, green (again) and blue. There are, indeed, two green channels because of the Bayer pattern of the camera's detector:
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

The image is thus shifted by half a pixel in the x and y directions between the two green channels, and thus they provide independent information about the image. The output images have 1640x1232 pixels per channel, with integer values from 0 to 1023.


# Install instructions

```bash
sudo apt-get install cmake libcfitsio-dev
git clone https://github.com/cschreib/picam2fits.git
cd picam2fits
mkdir build
cd build
cmake ../
make
sudo make install
```
