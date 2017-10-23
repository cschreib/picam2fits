#include <fstream>
#include <iostream>
#include <cstdint>
#include <string>
#include <vector>
#include <fitsio.h>

const std::size_t XNDPIX = 4128;
const std::size_t YNDPIX = 2480;
const std::size_t NDPIX = XNDPIX*YNDPIX;

const std::size_t PACK_NVAL = 4;
const std::size_t PACK_NBYTE = 5;
const std::size_t XNUPIX = XNDPIX*PACK_NVAL/PACK_NBYTE;
const std::size_t YNUPIX = YNDPIX;
const std::size_t NUPIX = XNUPIX*YNUPIX;

const std::size_t XNPIX = XNUPIX/2 - 11;
const std::size_t YNPIX = YNUPIX/2 - 8;
const std::size_t NPIX = XNPIX*YNPIX;

using pix_t = std::uint16_t;

bool cfitsio_check(int status) {
    if (status != 0) {
        char txt[FLEN_STATUS];
        fits_get_errstatus(status, txt);
        char tdetails[FLEN_ERRMSG];
        std::string details;
        while (fits_read_errmsg(tdetails)) {
            if (!details.empty()) details += "\ncfitsio: ";
            details += std::string(tdetails);
        }

        std::cerr << "error: cfitsio: " << std::string(txt) <<
            "\nerror: cfitsio: " << details << std::endl;
        return false;
    }

    return true;
}

template<typename F>
struct scoped {
    F func;

    template<typename U>
    scoped(U&& f) : func(std::move(f)) {}

    scoped(scoped&&) = default;
    scoped(const scoped&) = delete;

    ~scoped() {
        func();
    }
};

template<typename F>
scoped<typename std::decay<F>::type> make_scoped(F&& f) {
    return std::move(f);
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cout << "usage: picam2fits input.jpg output.fits" << std::endl;
        return 1;
    }

    std::string infile = argv[1];
    std::string outfile = argv[2];

    // Read RAW data as 8-bit unsigned integers
    std::vector<unsigned char> raw(NDPIX); {
        std::ifstream in(infile, std::ios::binary);

        in.seekg(0, std::ios::end);
        std::size_t fsize = in.tellg();

        if (fsize < NDPIX) {
            std::cerr << "error: file size is less than expected RAW data size" << std::endl;
            return 1;
        }

        in.seekg(fsize-NDPIX);
        in.read(reinterpret_cast<char*>(raw.data()), NDPIX);
    }

    // Unpack AAAAAAAA BBBBBBBB CCCCCCCC DDDDDDDD AABBCCDD
    //                                            ^^^^^^^^
    // Into   AAAAAAAAAA BBBBBBBBBB CCCCCCCCCC DDDDDDDDDD
    //                ^^         ^^         ^^         ^^
    std::vector<pix_t> unpacked(NUPIX); {
        for (std::size_t y = 0; y < YNDPIX; ++y)
        for (std::size_t x = 0; x < XNDPIX/PACK_NBYTE; ++x) {
            pix_t v5 = raw[y*XNDPIX+x*PACK_NBYTE+4];
            for (std::size_t p = 0; p < 4; ++p) {
                auto& v = unpacked[y*XNUPIX+x*PACK_NVAL+p];
                v = raw[y*XNDPIX+x*PACK_NBYTE+p];
                v <<= 2;
                v |= (v5 >> (6-p*2)) & 3;
            }
        }
    }

    // De-Bayer: BG  and flip Y-axis
    //           GR
    std::vector<pix_t> r(NPIX), g1(NPIX), g2(NPIX), b(NPIX); {
        for (std::size_t y = 0; y < YNPIX; ++y)
        for (std::size_t x = 0; x < XNPIX; ++x) {
            b[(YNPIX-1-y)*XNPIX+x]  = unpacked[(2*y+0)*XNUPIX+x*2+0];
            g1[(YNPIX-1-y)*XNPIX+x] = unpacked[(2*y+0)*XNUPIX+x*2+1];
            g2[(YNPIX-1-y)*XNPIX+x] = unpacked[(2*y+1)*XNUPIX+x*2+0];
            r[(YNPIX-1-y)*XNPIX+x]  = unpacked[(2*y+1)*XNUPIX+x*2+1];
        }
    }

    // Save to FITS file
    fitsfile* fptr = nullptr;
    int status = 0;
    auto deleter = make_scoped([&]() {
        if (fptr) {
            fits_close_file(fptr, &status);
        }
    });

    fits_create_file(&fptr, ("!"+outfile).c_str(), &status);
    if (!cfitsio_check(status)) return 1;

    // Empty primary HDU
    long naxes0 = 0;
    fits_insert_img(fptr, SHORT_IMG, 0, &naxes0, &status);
    if (!cfitsio_check(status)) return 1;

    // One HDU per chanel
    long naxes[] = {XNPIX, YNPIX};
    for (std::size_t i = 0; i < 4; ++i) {
        fits_insert_img(fptr, SHORT_IMG, 2, naxes, &status);
        if (!cfitsio_check(status)) return 1;
    }

    fits_set_hdustruc(fptr, &status);
    if (!cfitsio_check(status)) return 1;

    // R
    fits_movabs_hdu(fptr, 2, nullptr, &status);
    fits_write_img(fptr, TSHORT, 1, NPIX, r.data(), &status);
    if (!cfitsio_check(status)) return 1;

    // G
    fits_movabs_hdu(fptr, 3, nullptr, &status);
    fits_write_img(fptr, TSHORT, 1, NPIX, g1.data(), &status);
    if (!cfitsio_check(status)) return 1;

    // G
    fits_movabs_hdu(fptr, 4, nullptr, &status);
    fits_write_img(fptr, TSHORT, 1, NPIX, g2.data(), &status);
    if (!cfitsio_check(status)) return 1;

    // B
    fits_movabs_hdu(fptr, 5, nullptr, &status);
    fits_write_img(fptr, TSHORT, 1, NPIX, b.data(), &status);
    if (!cfitsio_check(status)) return 1;

    return 0;
}
