#include <fstream>
#include <iostream>
#include <sstream>
#include <cstdint>
#include <string>
#include <vector>
#include <fitsio.h>
#include <libexif/exif-data.h>

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

struct exif_data {
    ExifIfd current_ifd = EXIF_IFD_COUNT;
    ExifByteOrder current_byte_order;

    float exptime = 0;
    std::string dateobs;
};

template<typename T>
bool read_entry(ExifEntry* e, ExifByteOrder bo, T& f) {
    switch (e->format) {
    case EXIF_FORMAT_BYTE:
        f = *e->data;
        return true;
    case EXIF_FORMAT_SHORT:
        f = exif_get_short(e->data, bo);
        return true;
    case EXIF_FORMAT_LONG:
        f = exif_get_long(e->data, bo);
        return true;
    case EXIF_FORMAT_SSHORT:
        f = exif_get_sshort(e->data, bo);
        return true;
    case EXIF_FORMAT_SLONG:
        f = exif_get_slong(e->data, bo);
        return true;
    case EXIF_FORMAT_FLOAT:
        f = *reinterpret_cast<float*>(e->data);
        return true;
    case EXIF_FORMAT_DOUBLE:
        f = *reinterpret_cast<double*>(e->data);
        return true;
    case EXIF_FORMAT_RATIONAL: {
        ExifRational r = exif_get_rational(e->data, bo);
        f = double(r.numerator)/double(r.denominator);
        return true;
    }
    case EXIF_FORMAT_SRATIONAL: {
        ExifSRational r = exif_get_srational(e->data, bo);
        f = double(r.numerator)/double(r.denominator);
        return true;
    }
    case EXIF_FORMAT_ASCII: {
        std::istringstream iss(reinterpret_cast<char*>(e->data));
        return iss >> f;
    }
    case EXIF_FORMAT_UNDEFINED:
        return false;
    }
}

bool read_entry(ExifEntry* e, ExifByteOrder bo, std::string& f) {
    switch (e->format) {
    case EXIF_FORMAT_ASCII:
        f = std::string(reinterpret_cast<char*>(e->data), e->components);
        return true;
    case EXIF_FORMAT_UNDEFINED:
        return false;
    default: {
        double v;
        if (!read_entry(e, bo, v)) {
            return false;
        }

        std::ostringstream oss;
        oss << v;
        f = oss.str();

        return true;
    }
    }
}

void parse_exif_entry(ExifEntry* e, void* user_data) {
    exif_data& data = *static_cast<exif_data*>(user_data);

    // std::cout << "  " << exif_tag_get_name_in_ifd(e->tag, data.current_ifd)
    //           << " (fmt: " << e->format << ", cmp: " << e->components
    //           << ", id: " << std::hex << e->tag << std::dec << ")" << std::endl;

    switch (e->tag) {
    case EXIF_TAG_EXPOSURE_TIME:
        read_entry(e, data.current_byte_order, data.exptime);
        // std::cout << "    " << data.exptime << std::endl;
        break;
    case EXIF_TAG_DATE_TIME_ORIGINAL:
        read_entry(e, data.current_byte_order, data.dateobs);
        // std::cout << "    " << data.dateobs << std::endl;
        break;
    }
}

void parse_exif_content(ExifContent* c, void* user_data) {
    exif_data& data = *static_cast<exif_data*>(user_data);
    data.current_ifd = exif_content_get_ifd(c);

    // std::cout << data.current_ifd << std::endl;

    exif_content_foreach_entry(c, &parse_exif_entry, user_data);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "usage: picam2fits input.jpg ..." << std::endl;
        return 1;
    }

    for (int i = 1; i < argc; ++i) {
        std::string infile = argv[i];

        std::string outfile = infile; {
            // Change extension to fits
            auto p = outfile.find_last_of(".");
            if (p == outfile.npos) {
                outfile += ".fits";
            } else {
                outfile = outfile.substr(0, p)+".fits";
            }
        }

        ExifData* exif = exif_data_new_from_file(infile.c_str());
        auto exif_deleter = make_scoped([&]() {
            exif_data_unref(exif);
        });

        exif_data meta;
        meta.current_byte_order = exif_data_get_byte_order(exif);
        exif_data_foreach_content(exif, &parse_exif_content, static_cast<void*>(&meta));

        if (meta.current_ifd == EXIF_IFD_COUNT) {
            std::cerr << "error: " << infile << " does not appear to be a JPG file"
                << (argc > 2 ? ", ignoring" : "") << std::endl;
            continue;
        }

        // Read RAW data as 8-bit unsigned integers
        std::vector<unsigned char> raw(NDPIX); {
            std::ifstream in(infile, std::ios::binary);

            in.seekg(0, std::ios::end);
            std::size_t fsize = in.tellg();

            if (fsize < NDPIX) {
                std::cerr << "error: " << infile << " does not contain RAW data"
                    << (argc > 2 ? ", ignoring" : "") << std::endl;
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
        auto fits_deleter = make_scoped([&]() {
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

        auto write_wcs = [&](float crpix1, float crpix2) {
            const char* axis_type = "PIXEL";
            fits_write_key(fptr, TSTRING, "CTYPE1", const_cast<char*>(axis_type), nullptr, &status);
            if (!cfitsio_check(status)) return false;
            fits_write_key(fptr, TSTRING, "CTYPE2", const_cast<char*>(axis_type), nullptr, &status);
            if (!cfitsio_check(status)) return false;

            const char* unit = "um";
            fits_write_key(fptr, TSTRING, "CUNIT1", const_cast<char*>(unit), nullptr, &status);
            if (!cfitsio_check(status)) return false;
            fits_write_key(fptr, TSTRING, "CUNIT2", const_cast<char*>(unit), nullptr, &status);
            if (!cfitsio_check(status)) return false;

            fits_write_key(fptr, TFLOAT, "CRPIX1", &crpix1, nullptr, &status);
            if (!cfitsio_check(status)) return false;
            fits_write_key(fptr, TFLOAT, "CRPIX2", &crpix2, nullptr, &status);
            if (!cfitsio_check(status)) return false;

            float zero = 0.0;
            fits_write_key(fptr, TFLOAT, "CRVAL1", &zero, nullptr, &status);
            if (!cfitsio_check(status)) return false;
            fits_write_key(fptr, TFLOAT, "CRVAL2", &zero, nullptr, &status);
            if (!cfitsio_check(status)) return false;

            float pixel_size = 2*1.12;
            fits_write_key(fptr, TFLOAT, "CDELT1", &pixel_size, nullptr, &status);
            if (!cfitsio_check(status)) return false;
            fits_write_key(fptr, TFLOAT, "CDELT2", &pixel_size, nullptr, &status);
            if (!cfitsio_check(status)) return false;

            fits_write_key(fptr, TFLOAT, "EXPTIME", &meta.exptime, nullptr, &status);
            if (!cfitsio_check(status)) return false;
            fits_write_key(fptr, TSTRING, "DATEOBS",
                const_cast<char*>(meta.dateobs.c_str()), nullptr, &status);
            if (!cfitsio_check(status)) return false;

            return true;
        };

        // R
        fits_movabs_hdu(fptr, 2, nullptr, &status);
        fits_write_img(fptr, TSHORT, 1, NPIX, r.data(), &status);
        if (!cfitsio_check(status)) return 1;
        if (!write_wcs(0.0, 1.0)) return 1;

        // G
        fits_movabs_hdu(fptr, 3, nullptr, &status);
        fits_write_img(fptr, TSHORT, 1, NPIX, g1.data(), &status);
        if (!cfitsio_check(status)) return 1;
        if (!write_wcs(0.0, 0.5)) return 1;

        // G
        fits_movabs_hdu(fptr, 4, nullptr, &status);
        fits_write_img(fptr, TSHORT, 1, NPIX, g2.data(), &status);
        if (!cfitsio_check(status)) return 1;
        if (!write_wcs(0.5, 1.0)) return 1;

        // B
        fits_movabs_hdu(fptr, 5, nullptr, &status);
        fits_write_img(fptr, TSHORT, 1, NPIX, b.data(), &status);
        if (!cfitsio_check(status)) return 1;
        if (!write_wcs(0.5, 0.5)) return 1;
    }

    return 0;
}
