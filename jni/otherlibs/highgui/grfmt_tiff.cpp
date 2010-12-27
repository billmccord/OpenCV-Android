/*M///////////////////////////////////////////////////////////////////////////////////////
//
//  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
//
//  By downloading, copying, installing or using the software you agree to this license.
//  If you do not agree to this license, do not download, install,
//  copy or use the software.
//
//
//                        Intel License Agreement
//                For Open Source Computer Vision Library
//
// Copyright (C) 2000, Intel Corporation, all rights reserved.
// Third party copyrights are property of their respective owners.
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
//   * Redistribution's of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//
//   * Redistribution's in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//
//   * The name of Intel Corporation may not be used to endorse or promote products
//     derived from this software without specific prior written permission.
//
// This software is provided by the copyright holders and contributors "as is" and
// any express or implied warranties, including, but not limited to, the implied
// warranties of merchantability and fitness for a particular purpose are disclaimed.
// In no event shall the Intel Corporation or contributors be liable for any direct,
// indirect, incidental, special, exemplary, or consequential damages
// (including, but not limited to, procurement of substitute goods or services;
// loss of use, data, or profits; or business interruption) however caused
// and on any theory of liability, whether in contract, strict liability,
// or tort (including negligence or otherwise) arising in any way out of
// the use of this software, even if advised of the possibility of such damage.
//
//M*/

/****************************************************************************************\
    A part of the file implements TIFF reader on base of libtiff library
    (see otherlibs/_graphics/readme.txt for copyright notice)
\****************************************************************************************/

#include "_highgui.h"
#include "grfmt_tiff.h"

static const char fmtSignTiffII[] = "II\x2a\x00";
static const char fmtSignTiffMM[] = "MM\x00\x2a";

GrFmtTiff::GrFmtTiff()
{
    m_sign_len = 4;
    m_signature = "";
    m_description = "TIFF Files (*.tiff;*.tif)";
}

GrFmtTiff::~GrFmtTiff()
{
}

bool GrFmtTiff::CheckSignature( const char* signature )
{
    return memcmp( signature, fmtSignTiffII, 4 ) == 0 ||
           memcmp( signature, fmtSignTiffMM, 4 ) == 0;
}


GrFmtReader* GrFmtTiff::NewReader( const char* filename )
{
    return new GrFmtTiffReader( filename );
}


GrFmtWriter* GrFmtTiff::NewWriter( const char* filename )
{
    return new GrFmtTiffWriter( filename );
}


#ifdef HAVE_TIFF

#include "tiff.h"
#include "tiffio.h"

static int grfmt_tiff_err_handler_init = 0;

static void GrFmtSilentTIFFErrorHandler( const char*, const char*, va_list ) {}

GrFmtTiffReader::GrFmtTiffReader( const char* filename ) : GrFmtReader( filename )
{
    m_tif = 0;

    if( !grfmt_tiff_err_handler_init )
    {
        grfmt_tiff_err_handler_init = 1;

        TIFFSetErrorHandler( GrFmtSilentTIFFErrorHandler );
        TIFFSetWarningHandler( GrFmtSilentTIFFErrorHandler );
    }
}


GrFmtTiffReader::~GrFmtTiffReader()
{
}


void  GrFmtTiffReader::Close()
{
    if( m_tif )
    {
        TIFF* tif = (TIFF*)m_tif;
        TIFFClose( tif );
        m_tif = 0;
    }
}


bool  GrFmtTiffReader::CheckFormat( const char* signature )
{
    return memcmp( signature, fmtSignTiffII, 4 ) == 0 ||
           memcmp( signature, fmtSignTiffMM, 4 ) == 0;
}


bool  GrFmtTiffReader::ReadHeader()
{
    char errmsg[1024];
    bool result = false;

    Close();
    TIFF* tif = TIFFOpen( m_filename, "r" );

    if( tif )
    {
        int width = 0, height = 0, photometric = 0, compression = 0;
        m_tif = tif;

        if( TIFFRGBAImageOK( tif, errmsg ) &&
            TIFFGetField( tif, TIFFTAG_IMAGEWIDTH, &width ) &&
            TIFFGetField( tif, TIFFTAG_IMAGELENGTH, &height ) &&
            TIFFGetField( tif, TIFFTAG_PHOTOMETRIC, &photometric ) && 
            (!TIFFGetField( tif, TIFFTAG_COMPRESSION, &compression ) ||
            (compression != COMPRESSION_LZW &&
             compression != COMPRESSION_OJPEG)))
        {
            m_width = width;
            m_height = height;
            m_iscolor = photometric > 1;
            
            result = true;
        }
    }

    if( !result )
        Close();

    return result;
}


bool  GrFmtTiffReader::ReadData( uchar* data, int step, int color )
{
    bool result = false;
    uchar* buffer = 0;

    color = color > 0 || (color < 0 && m_iscolor);

    if( m_tif && m_width && m_height )
    {
        TIFF* tif = (TIFF*)m_tif;
        int tile_width0 = m_width, tile_height0 = 0;
        int x, y, i;
        int is_tiled = TIFFIsTiled(tif);
        
        if( !is_tiled &&
            TIFFGetField( tif, TIFFTAG_ROWSPERSTRIP, &tile_height0 ) ||
            is_tiled &&
            TIFFGetField( tif, TIFFTAG_TILEWIDTH, &tile_width0 ) &&
            TIFFGetField( tif, TIFFTAG_TILELENGTH, &tile_height0 ))
        {
            if( tile_width0 <= 0 )
                tile_width0 = m_width;

            if( tile_height0 <= 0 )
                tile_height0 = m_height;
            
            buffer = new uchar[tile_height0*tile_width0*4];

            for( y = 0; y < m_height; y += tile_height0, data += step*tile_height0 )
            {
                int tile_height = tile_height0;

                if( y + tile_height > m_height )
                    tile_height = m_height - y;

                for( x = 0; x < m_width; x += tile_width0 )
                {
                    int tile_width = tile_width0, ok;

                    if( x + tile_width > m_width )
                        tile_width = m_width - x;

                    if( !is_tiled )
                        ok = TIFFReadRGBAStrip( tif, y, (uint32*)buffer );
                    else
                        ok = TIFFReadRGBATile( tif, x, y, (uint32*)buffer );

                    if( !ok )
                        goto exit_func;

                    for( i = 0; i < tile_height; i++ )
                        if( color )
                            icvCvt_BGRA2BGR_8u_C4C3R( buffer + i*tile_width*4, 0,
                                          data + x*3 + step*(tile_height - i - 1), 0,
                                          cvSize(tile_width,1), 2 );
                        else
                            icvCvt_BGRA2Gray_8u_C4C1R( buffer + i*tile_width*4, 0,
                                           data + x + step*(tile_height - i - 1), 0,
                                           cvSize(tile_width,1), 2 );
                }
            }

            result = true;
        }
    }

exit_func:

    Close();
    delete[] buffer;

    return result;
}

#else

static const int  tiffMask[] = { 0xff, 0xff, 0xffffffff, 0xffff, 0xffffffff };

/************************ TIFF reader *****************************/

GrFmtTiffReader::GrFmtTiffReader( const char* filename ) : GrFmtReader( filename )
{
    m_offsets = 0;
    m_maxoffsets = 0;
    m_strips = -1;
    m_max_pal_length = 0;
    m_temp_palette = 0;
}


GrFmtTiffReader::~GrFmtTiffReader()
{
    Close();

    delete[] m_offsets;
    delete[] m_temp_palette;
}

void  GrFmtTiffReader::Close()
{
    m_strm.Close();
}


bool  GrFmtTiffReader::CheckFormat( const char* signature )
{
    return memcmp( signature, fmtSignTiffII, 4 ) == 0 ||
           memcmp( signature, fmtSignTiffMM, 4 ) == 0;
}


int   GrFmtTiffReader::GetWordEx()
{
    int val = m_strm.GetWord();
    if( m_byteorder == TIFF_ORDER_MM )
        val = ((val)>>8)|(((val)&0xff)<<8);
    return val;
}


int   GrFmtTiffReader::GetDWordEx()
{
    int val = m_strm.GetDWord();
    if( m_byteorder == TIFF_ORDER_MM )
        val = BSWAP( val );
    return val;
}


int  GrFmtTiffReader::ReadTable( int offset, int count,
                                 TiffFieldType fieldType,
                                 int*& array, int& arraysize )
{
    int i;
    
    if( count < 0 )
        return RBS_BAD_HEADER;
    
    if( fieldType != TIFF_TYPE_SHORT &&
        fieldType != TIFF_TYPE_LONG &&
        fieldType != TIFF_TYPE_BYTE )
        return RBS_BAD_HEADER;

    if( count > arraysize )
    {
        delete[] array;
        arraysize = arraysize*3/2;
        if( arraysize < count )
            arraysize = count;
        array = new int[arraysize];
    }

    if( count > 1 )
    {
        int pos = m_strm.GetPos();
        m_strm.SetPos( offset );

        if( fieldType == TIFF_TYPE_LONG )
        {
            if( m_byteorder == TIFF_ORDER_MM )
                for( i = 0; i < count; i++ )
                    array[i] = ((RMByteStream&)m_strm).GetDWord();
            else
                for( i = 0; i < count; i++ )
                    array[i] = ((RLByteStream&)m_strm).GetDWord();
        }
        else if( fieldType == TIFF_TYPE_SHORT )
        {
            if( m_byteorder == TIFF_ORDER_MM )
                for( i = 0; i < count; i++ )
                    array[i] = ((RMByteStream&)m_strm).GetWord();
            else
                for( i = 0; i < count; i++ )
                    array[i] = ((RLByteStream&)m_strm).GetWord();
        }
        else // fieldType == TIFF_TYPE_BYTE
            for( i = 0; i < count; i++ )
                array[i] = m_strm.GetByte();

        m_strm.SetPos(pos);
    }
    else
    {
        assert( (offset & ~tiffMask[fieldType]) == 0 );
        array[0] = offset;
    }

    return 0;
}


bool  GrFmtTiffReader::ReadHeader()
{
    bool result = false;
    int  photometric = -1;
    int  channels = 1;
    int  pal_length = -1;

    const int MAX_CHANNELS = 4;
    int  bpp_arr[MAX_CHANNELS];

    assert( strlen(m_filename) != 0 );
    if( !m_strm.Open( m_filename )) return false;

    m_width = -1;
    m_height = -1;
    m_strips = -1;
    m_bpp = 1;
    m_compression = TIFF_UNCOMP;
    m_rows_per_strip = -1;
    m_iscolor = false;

    if( setjmp( m_strm.JmpBuf()) == 0 )
    {
        m_byteorder = (TiffByteOrder)m_strm.GetWord();
        m_strm.Skip( 2 );
        int header_offset = GetDWordEx();

        m_strm.SetPos( header_offset );

        // read the first tag directory
        int i, j, count = GetWordEx();

        for( i = 0; i < count; i++ )
        {
            // read tag
            TiffTag tag = (TiffTag)GetWordEx();
            TiffFieldType fieldType = (TiffFieldType)GetWordEx();
            int count = GetDWordEx();
            int value = GetDWordEx();
            if( count == 1 )
            {
                if( m_byteorder == TIFF_ORDER_MM )
                {
                    if( fieldType == TIFF_TYPE_SHORT )
                        value = (unsigned)value >> 16;
                    else if( fieldType == TIFF_TYPE_BYTE )
                        value = (unsigned)value >> 24;
                }

                value &= tiffMask[fieldType];
            }

            switch( tag )
            {
            case  TIFF_TAG_WIDTH:
                m_width = value;
                break;

            case  TIFF_TAG_HEIGHT:
                m_height = value;
                break;

            case  TIFF_TAG_BITS_PER_SAMPLE:
                {
                    int* bpp_arr_ref = bpp_arr;

                    if( count > MAX_CHANNELS )
                        BAD_HEADER_ERR();

                    if( ReadTable( value, count, fieldType, bpp_arr_ref, count ) < 0 )
                        BAD_HEADER_ERR();
                
                    for( j = 1; j < count; j++ )
                    {
                        if( bpp_arr[j] != bpp_arr[0] )
                            BAD_HEADER_ERR();
                    }

                    m_bpp = bpp_arr[0];
                }

                break;

            case  TIFF_TAG_COMPRESSION:
                m_compression = (TiffCompression)value;
                if( m_compression != TIFF_UNCOMP &&
                    m_compression != TIFF_HUFFMAN &&
                    m_compression != TIFF_PACKBITS )
                    BAD_HEADER_ERR();
                break;

            case  TIFF_TAG_PHOTOMETRIC:
                photometric = value;
                if( (unsigned)photometric > 3 )
                    BAD_HEADER_ERR();
                break;

            case  TIFF_TAG_STRIP_OFFSETS:
                m_strips = count;
                if( ReadTable( value, count, fieldType, m_offsets, m_maxoffsets ) < 0 )
                    BAD_HEADER_ERR();
                break;

            case  TIFF_TAG_SAMPLES_PER_PIXEL:
                channels = value;
                if( channels != 1 && channels != 3 && channels != 4 )
                    BAD_HEADER_ERR();
                break;

            case  TIFF_TAG_ROWS_PER_STRIP:
                m_rows_per_strip = value;
                break;

            case  TIFF_TAG_PLANAR_CONFIG:
                {
                int planar_config = value;
                if( planar_config != 1 )
                    BAD_HEADER_ERR();
                }
                break;

            case  TIFF_TAG_COLOR_MAP:
                if( fieldType != TIFF_TYPE_SHORT || count < 2 )
                    BAD_HEADER_ERR();
                if( ReadTable( value, count, fieldType,
                               m_temp_palette, m_max_pal_length ) < 0 )
                    BAD_HEADER_ERR();
                pal_length = count / 3;
                if( pal_length > 256 )
                    BAD_HEADER_ERR();
                for( i = 0; i < pal_length; i++ )
                {
                    m_palette[i].r = (uchar)(m_temp_palette[i] >> 8);
                    m_palette[i].g = (uchar)(m_temp_palette[i + pal_length] >> 8);
                    m_palette[i].b = (uchar)(m_temp_palette[i + pal_length*2] >> 8);
                }
                break;
            case  TIFF_TAG_STRIP_COUNTS:
                break;
            }
        }

        if( m_strips == 1 && m_rows_per_strip == -1 )
            m_rows_per_strip = m_height;

        if( m_width > 0 && m_height > 0 && m_strips > 0 &&
            (m_height + m_rows_per_strip - 1)/m_rows_per_strip == m_strips )
        {
            switch( m_bpp )
            {
            case 1:
                if( photometric == 0 || photometric == 1 && channels == 1 )
                {
                    FillGrayPalette( m_palette, m_bpp, photometric == 0 );
                    result = true;
                    m_iscolor = false;
                }
                break;
            case 4:
            case 8:
                if( (photometric == 0 || photometric == 1 ||
                     photometric == 3 && pal_length == (1 << m_bpp)) &&
                    m_compression != TIFF_HUFFMAN && channels == 1 )
                {
                    if( pal_length < 0 )
                    {
                        FillGrayPalette( m_palette, m_bpp, photometric == 0 );
                        m_iscolor = false;
                    }
                    else
                    {
                        m_iscolor = IsColorPalette( m_palette, m_bpp );
                    }
                    result = true;
                }
                else if( photometric == 2 && pal_length < 0 &&
                         (channels == 3 || channels == 4) &&
                         m_compression == TIFF_UNCOMP )
                {
                    m_bpp = 8*channels;
                    m_iscolor = true;
                    result = true;
                }
                break;
            default:
                BAD_HEADER_ERR();
            }
        }
bad_header_exit:
        ;
    }

    if( !result )
    {
        m_strips = -1;
        m_width = m_height = -1;
        m_strm.Close();
    }

    return result;
}


bool  GrFmtTiffReader::ReadData( uchar* data, int step, int color )
{
    const  int buffer_size = 1 << 12;
    uchar  buffer[buffer_size];
    uchar  gray_palette[256];
    bool   result = false;
    uchar* src = buffer;
    int    src_pitch = (m_width*m_bpp + 7)/8;
    int    y = 0;

    if( m_strips < 0 || !m_strm.IsOpened())
        return false;
    
    if( src_pitch+32 > buffer_size )
        src = new uchar[src_pitch+32];

    if( !color )
        if( m_bpp <= 8 )
        {
            CvtPaletteToGray( m_palette, gray_palette, 1 << m_bpp );
        }

    if( setjmp( m_strm.JmpBuf()) == 0 )
    {
        for( int s = 0; s < m_strips; s++ )
        {
            int y_limit = m_rows_per_strip;

            y_limit += y;
            if( y_limit > m_height ) y_limit = m_height;

            m_strm.SetPos( m_offsets[s] );

            if( m_compression == TIFF_UNCOMP )
            {
                for( ; y < y_limit; y++, data += step )
                {
                    m_strm.GetBytes( src, src_pitch );
                    if( color )
                        switch( m_bpp )
                        {
                        case 1:
                            FillColorRow1( data, src, m_width, m_palette );
                            break;
                        case 4:
                            FillColorRow4( data, src, m_width, m_palette );
                            break;
                        case 8:
                            FillColorRow8( data, src, m_width, m_palette );
                            break;
                        case 24:
                            icvCvt_RGB2BGR_8u_C3R( src, 0, data, 0, cvSize(m_width,1) );
                            break;
                        case 32:
                            icvCvt_BGRA2BGR_8u_C4C3R( src, 0, data, 0, cvSize(m_width,1), 2 );
                            break;
                        default:
                            assert(0);
                            goto bad_decoding_end;
                        }
                    else
                        switch( m_bpp )
                        {
                        case 1:
                            FillGrayRow1( data, src, m_width, gray_palette );
                            break;
                        case 4:
                            FillGrayRow4( data, src, m_width, gray_palette );
                            break;
                        case 8:
                            FillGrayRow8( data, src, m_width, gray_palette );
                            break;
                        case 24:
                            icvCvt_BGR2Gray_8u_C3C1R( src, 0, data, 0, cvSize(m_width,1), 2 );
                            break;
                        case 32:
                            icvCvt_BGRA2Gray_8u_C4C1R( src, 0, data, 0, cvSize(m_width,1), 2 );
                            break;
                        default:
                            assert(0);
                            goto bad_decoding_end;
                        }
                }
            }
            else
            {
            }

            result = true;

bad_decoding_end:

            ;
        }
    }

    if( src != buffer ) delete[] src; 
    return result;
}

#endif

//////////////////////////////////////////////////////////////////////////////////////////

GrFmtTiffWriter::GrFmtTiffWriter( const char* filename ) : GrFmtWriter( filename )
{
}

GrFmtTiffWriter::~GrFmtTiffWriter()
{
}

void  GrFmtTiffWriter::WriteTag( TiffTag tag, TiffFieldType fieldType,
                                 int count, int value )
{
    m_strm.PutWord( tag );
    m_strm.PutWord( fieldType );
    m_strm.PutDWord( count );
    m_strm.PutDWord( value );
}


bool  GrFmtTiffWriter::WriteImage( const uchar* data, int step,
                                   int width, int height, int /*depth*/, int channels )
{
    bool result = false;
    int fileStep = width*channels;

    assert( data && width > 0 && height > 0 && step >= fileStep);

    if( m_strm.Open( m_filename ) )
    {
        int rowsPerStrip = (1 << 13)/fileStep;

        if( rowsPerStrip < 1 )
            rowsPerStrip = 1;

        if( rowsPerStrip > height )
            rowsPerStrip = height;

        int i, stripCount = (height + rowsPerStrip - 1) / rowsPerStrip;
/*#if defined _DEBUG || !defined WIN32
        int uncompressedRowSize = rowsPerStrip * fileStep;
#endif*/
        int directoryOffset = 0;

        int* stripOffsets = new int[stripCount];
        short* stripCounts = new short[stripCount];
        uchar* buffer = new uchar[fileStep + 32];
        int  stripOffsetsOffset = 0;
        int  stripCountsOffset = 0;
        int  bitsPerSample = 8; // TODO support 16 bit
        int  y = 0;

        m_strm.PutBytes( fmtSignTiffII, 4 );
        m_strm.PutDWord( directoryOffset );

        // write an image data first (the most reasonable way
        // for compressed images)
        for( i = 0; i < stripCount; i++ )
        {
            int limit = y + rowsPerStrip;

            if( limit > height )
                limit = height;

            stripOffsets[i] = m_strm.GetPos();

            for( ; y < limit; y++, data += step )
            {
                if( channels == 3 )
                    icvCvt_BGR2RGB_8u_C3R( data, 0, buffer, 0, cvSize(width,1) );
                else if( channels == 4 )
                    icvCvt_BGRA2RGBA_8u_C4R( data, 0, buffer, 0, cvSize(width,1) );

                m_strm.PutBytes( channels > 1 ? buffer : data, fileStep );
            }

            stripCounts[i] = (short)(m_strm.GetPos() - stripOffsets[i]);
            /*assert( stripCounts[i] == uncompressedRowSize ||
                    stripCounts[i] < uncompressedRowSize &&
                    i == stripCount - 1);*/
        }

        if( stripCount > 2 )
        {
            stripOffsetsOffset = m_strm.GetPos();
            for( i = 0; i < stripCount; i++ )
                m_strm.PutDWord( stripOffsets[i] );

            stripCountsOffset = m_strm.GetPos();
            for( i = 0; i < stripCount; i++ )
                m_strm.PutWord( stripCounts[i] );
        }
        else if(stripCount == 2)
        {
            stripOffsetsOffset = m_strm.GetPos();
            for (i = 0; i < stripCount; i++)
            {
                m_strm.PutDWord (stripOffsets [i]);
            }
            stripCountsOffset = stripCounts [0] + (stripCounts [1] << 16);
        }
        else
        {
            stripOffsetsOffset = stripOffsets[0];
            stripCountsOffset = stripCounts[0];
        }

        if( channels > 1 )
        {
            bitsPerSample = m_strm.GetPos();
            m_strm.PutWord(8);
            m_strm.PutWord(8);
            m_strm.PutWord(8);
            if( channels == 4 )
                m_strm.PutWord(8);
        }

        directoryOffset = m_strm.GetPos();

        // write header
        m_strm.PutWord( 9 );

        /* warning: specification 5.0 of Tiff want to have tags in
           ascending order. This is a non-fatal error, but this cause
           warning with some tools. So, keep this in ascending order */

        WriteTag( TIFF_TAG_WIDTH, TIFF_TYPE_LONG, 1, width );
        WriteTag( TIFF_TAG_HEIGHT, TIFF_TYPE_LONG, 1, height );
        WriteTag( TIFF_TAG_BITS_PER_SAMPLE,
                  TIFF_TYPE_SHORT, channels, bitsPerSample );
        WriteTag( TIFF_TAG_COMPRESSION, TIFF_TYPE_LONG, 1, TIFF_UNCOMP );
        WriteTag( TIFF_TAG_PHOTOMETRIC, TIFF_TYPE_SHORT, 1, channels > 1 ? 2 : 1 );

        WriteTag( TIFF_TAG_STRIP_OFFSETS, TIFF_TYPE_LONG,
                  stripCount, stripOffsetsOffset );

        WriteTag( TIFF_TAG_SAMPLES_PER_PIXEL, TIFF_TYPE_SHORT, 1, channels );
        WriteTag( TIFF_TAG_ROWS_PER_STRIP, TIFF_TYPE_LONG, 1, rowsPerStrip );
        
        WriteTag( TIFF_TAG_STRIP_COUNTS,
                  stripCount > 1 ? TIFF_TYPE_SHORT : TIFF_TYPE_LONG,
                  stripCount, stripCountsOffset );

        m_strm.PutDWord(0);
        m_strm.Close();

        // write directory offset
        FILE* f = fopen( m_filename, "r+b" );
        buffer[0] = (uchar)directoryOffset;
        buffer[1] = (uchar)(directoryOffset >> 8);
        buffer[2] = (uchar)(directoryOffset >> 16);
        buffer[3] = (uchar)(directoryOffset >> 24);

        fseek( f, 4, SEEK_SET );
        fwrite( buffer, 1, 4, f );
        fclose(f);

        delete[]  stripOffsets;
        delete[]  stripCounts;
        delete[] buffer;

        result = true;
    }
    return result;
}

