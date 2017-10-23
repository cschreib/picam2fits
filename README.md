# picam2fits

This is a simple command line tool to convert shots taken with the Raspberry Pi Camera v2 into FITS images. The shots must be taken with the ```--raw``` option of ```raspistill```:
```bash
raspistill --raw -o image.jpg
picam2fits image.jpg image.fits
```

The generated FITS file contains four extensions (plus the primary HDU, which is empty), one for each color channel: red, green, green (again) and blue. There are, indeed, two green channels because of the Bayer pattern of the camera's detector. They contain redundant but independent information, and are thus not combined into a single channel.

# Install instructions

```bash
sudo apt-get install libcfitsio-dev
git clone https://github.com/cschreib/picam2fits.git
cd picam2fits
mkdir build
cd build
cmake ../
make
sudo make install
```
