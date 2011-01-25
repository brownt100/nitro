/* =========================================================================
 * This file is part of NITRO
 * =========================================================================
 *
 * (C) Copyright 2004 - 2010, General Dynamics - Advanced Information Systems
 *
 * NITRO is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, If not,
 * see <http://www.gnu.org/licenses/>.
 *
 */

#ifdef HAVE_OPENJPEG_H

#include "j2k/Container.h"
#include "j2k/Reader.h"
#include "j2k/Writer.h"

#include <openjpeg.h>


/******************************************************************************/
/* TYPES & DECLARATIONS                                                       */
/******************************************************************************/
typedef struct _IOControl
{
    nrt_IOInterface *io;
    nrt_Off offset;
    nrt_Off length;
    int isRead;
    nrt_Error error;
} IOControl;


typedef struct _OpenJPEGReaderImpl
{
    opj_dparameters_t parameters;
    nrt_Off ioOffset;
    nrt_IOInterface *io;
    int ownIO;
    j2k_Container *container;
} OpenJPEGReaderImpl;

typedef struct _OpenJPEGWriterImpl
{
    j2k_Container *container;
    opj_codec_t *codec;
    opj_image_t *image;
    char *compressedBuf;
    nrt_IOInterface *compressed;
    opj_stream_t *stream;
} OpenJPEGWriterImpl;

typedef struct _OpenJPEGContainerImpl
{
    nrt_Int32 x0;
    nrt_Int32 y0;
    nrt_Uint32 tileWidth;
    nrt_Uint32 tileHeight;
    nrt_Uint32 xTiles;
    nrt_Uint32 yTiles;
    nrt_Uint32 width;
    nrt_Uint32 height;
    nrt_Uint32 nComponents;
    nrt_Uint32 componentBytes;
    nrt_Uint32 inputComponentBits;
    int imageType;
    int isSigned;
} OpenJPEGContainerImpl;

J2KPRIV(OPJ_UINT32) implStreamRead(void* buf, OPJ_UINT32 bytes, void *data);
J2KPRIV(bool)       implStreamSeek(OPJ_SIZE_T bytes, void *data);
J2KPRIV(OPJ_SIZE_T) implStreamSkip(OPJ_SIZE_T bytes, void *data);
J2KPRIV(OPJ_UINT32) implStreamWrite(void *buf, OPJ_UINT32 bytes, void *data);


J2KPRIV( nrt_Uint32) OpenJPEGContainer_getTilesX(J2K_USER_DATA *, nrt_Error *);
J2KPRIV( nrt_Uint32) OpenJPEGContainer_getTilesY(J2K_USER_DATA *, nrt_Error *);
J2KPRIV( nrt_Uint32) OpenJPEGContainer_getTileWidth(J2K_USER_DATA *, nrt_Error *);
J2KPRIV( nrt_Uint32) OpenJPEGContainer_getTileHeight(J2K_USER_DATA *, nrt_Error *);
J2KPRIV( nrt_Uint32) OpenJPEGContainer_getWidth(J2K_USER_DATA *, nrt_Error *);
J2KPRIV( nrt_Uint32) OpenJPEGContainer_getHeight(J2K_USER_DATA *, nrt_Error *);
J2KPRIV( nrt_Uint32) OpenJPEGContainer_getNumComponents(J2K_USER_DATA *, nrt_Error *);
J2KPRIV( nrt_Uint32) OpenJPEGContainer_getComponentBytes(J2K_USER_DATA *, nrt_Error *);
J2KPRIV( nrt_Uint32) OpenJPEGContainer_getComponentBits(J2K_USER_DATA *, nrt_Error *);
J2KPRIV( int)        OpenJPEGContainer_getImageType(J2K_USER_DATA *, nrt_Error *);
J2KPRIV( J2K_BOOL)   OpenJPEGContainer_isSigned(J2K_USER_DATA *, nrt_Error *);
J2KPRIV(void)        OpenJPEGContainer_destruct(J2K_USER_DATA *);

static j2k_IContainer ContainerInterface = { &OpenJPEGContainer_getTilesX,
                                             &OpenJPEGContainer_getTilesY,
                                             &OpenJPEGContainer_getTileWidth,
                                             &OpenJPEGContainer_getTileHeight,
                                             &OpenJPEGContainer_getWidth,
                                             &OpenJPEGContainer_getHeight,
                                             &OpenJPEGContainer_getNumComponents,
                                             &OpenJPEGContainer_getComponentBytes,
                                             &OpenJPEGContainer_getComponentBits,
                                             &OpenJPEGContainer_getImageType,
                                             &OpenJPEGContainer_isSigned,
                                             &OpenJPEGContainer_destruct};

J2KPRIV( NRT_BOOL  )     OpenJPEGReader_canReadTiles(J2K_USER_DATA *,  nrt_Error *);
J2KPRIV( nrt_Uint64)     OpenJPEGReader_readTile(J2K_USER_DATA *, nrt_Uint32,
                                                 nrt_Uint32, nrt_Uint8 **,
                                                 nrt_Error *);
J2KPRIV( nrt_Uint64)     OpenJPEGReader_readRegion(J2K_USER_DATA *, nrt_Uint32,
                                                   nrt_Uint32, nrt_Uint32,
                                                   nrt_Uint32, nrt_Uint8 **,
                                                   nrt_Error *);
J2KPRIV( j2k_Container*) OpenJPEGReader_getContainer(J2K_USER_DATA *, nrt_Error *);
J2KPRIV(void)            OpenJPEGReader_destruct(J2K_USER_DATA *);

static j2k_IReader ReaderInterface = {&OpenJPEGReader_canReadTiles,
                                      &OpenJPEGReader_readTile,
                                      &OpenJPEGReader_readRegion,
                                      &OpenJPEGReader_getContainer,
                                      &OpenJPEGReader_destruct };

J2KPRIV( NRT_BOOL)       OpenJPEGWriter_setTile(J2K_USER_DATA *,
                                                nrt_Uint32, nrt_Uint32,
                                                nrt_Uint8 *, nrt_Uint32,
                                                nrt_Error *);
J2KPRIV( NRT_BOOL)       OpenJPEGWriter_write(J2K_USER_DATA *, nrt_IOInterface *,
                                              nrt_Error *);
J2KPRIV( j2k_Container*) OpenJPEGWriter_getContainer(J2K_USER_DATA *, nrt_Error *);
J2KPRIV(void)            OpenJPEGWriter_destruct(J2K_USER_DATA *);

static j2k_IWriter WriterInterface = {&OpenJPEGWriter_setTile,
                                      &OpenJPEGWriter_write,
                                      &OpenJPEGWriter_getContainer,
                                      &OpenJPEGWriter_destruct };


J2KPRIV(void) OpenJPEG_cleanup(opj_stream_t **, opj_codec_t **, opj_image_t **);
J2KPRIV( J2K_BOOL) OpenJPEG_initImage(OpenJPEGWriterImpl *, nrt_Error *);

/******************************************************************************/
/* IO                                                                         */
/******************************************************************************/

J2KAPI(opj_stream_t*)
OpenJPEG_createIO(nrt_IOInterface *io, nrt_Off length, int isInput,
                  nrt_Error *error)
{
    opj_stream_t *stream = NULL;
    IOControl *ioControl = NULL;

    stream = opj_stream_create(1024, isInput);
    if (!stream)
    {
        nrt_Error_init(error, "Error creating openjpeg stream", NRT_CTXT,
                       NRT_ERR_MEMORY);
    }
    else
    {
        /*      Allocate the result */
        ioControl = (IOControl *) J2K_MALLOC(sizeof(IOControl));
        if (ioControl == NULL)
        {
            nrt_Error_init(error, "Error creating control object", NRT_CTXT,
                           NRT_ERR_MEMORY);
            return NULL;
        }
        ioControl->io = io;
        ioControl->offset = nrt_IOInterface_tell(io, error);
        if (length > 0)
            ioControl->length = length;
        else
            ioControl->length = nrt_IOInterface_getSize(io, error)
                    - ioControl->offset;

        opj_stream_set_user_data(stream, ioControl);
        opj_stream_set_read_function(stream,
                                     (opj_stream_read_fn) implStreamRead);
        opj_stream_set_seek_function(stream,
                                     (opj_stream_seek_fn) implStreamSeek);
        opj_stream_set_skip_function(stream,
                                     (opj_stream_skip_fn) implStreamSkip);
        opj_stream_set_write_function(stream,
                                     (opj_stream_write_fn) implStreamWrite);
    }
    return stream;
}

J2KPRIV(OPJ_UINT32) implStreamRead(void* buf, OPJ_UINT32 bytes, void *data)
{
    IOControl *ctrl = (IOControl*)data;
    nrt_Off offset, bytesLeft, alreadyRead, len;
    OPJ_UINT32 toRead;

    offset = nrt_IOInterface_tell(ctrl->io, &ctrl->error);
    assert(offset >= ctrl->offset); /* probably not a good idea, but need it */

    alreadyRead = offset - ctrl->offset;
    bytesLeft = alreadyRead >= ctrl->length ? 0 : ctrl->length - alreadyRead;
    toRead = bytesLeft < (nrt_Off)bytes ? (OPJ_UINT32)bytesLeft : bytes;
    if (toRead <= 0 || !nrt_IOInterface_read(
                    ctrl->io, (char*)buf, toRead, &ctrl->error))
    {
        return 0;
    }
    return toRead;
}

J2KPRIV(bool) implStreamSeek(OPJ_SIZE_T bytes, void *data)
{
    IOControl *ctrl = (IOControl*)data;
    if (!nrt_IOInterface_seek(ctrl->io, ctrl->offset + bytes,
                    NRT_SEEK_SET, &ctrl->error))
    {
        return 0;
    }
    return 1;
}

J2KPRIV(OPJ_SIZE_T) implStreamSkip(OPJ_SIZE_T bytes, void *data)
{
    IOControl *ctrl = (IOControl*)data;
    if (bytes < 0)
    {
        return 0;
    }
    if (!nrt_IOInterface_seek(ctrl->io, bytes, NRT_SEEK_CUR, &ctrl->error))
    {
        return -1;
    }
    return bytes;
}

J2KPRIV(OPJ_UINT32) implStreamWrite(void *buf, OPJ_UINT32 bytes, void *data)
{
    IOControl *ctrl = (IOControl*)data;
    if (bytes == 0)
    {
        return 0;
    }
    if (!nrt_IOInterface_write(ctrl->io, (const char*)buf,
                               (size_t)bytes, &ctrl->error))
    {
        return -1;
    }
    return bytes;
}

J2KPRIV(void)
OpenJPEG_cleanup(opj_stream_t **stream, opj_codec_t **codec,
                  opj_image_t **image)
{
    if (stream && *stream)
    {
        opj_stream_destroy(*stream);
        *stream = NULL;
    }
    if (codec && *codec)
    {
        opj_destroy_codec(*codec);
        *codec = NULL;
    }
    if (image && *image)
    {
        opj_image_destroy(*image);
        *image = NULL;
    }
}

/******************************************************************************/
/* UTILITIES                                                                  */
/******************************************************************************/

J2KPRIV( NRT_BOOL)
OpenJPEG_setup(OpenJPEGReaderImpl *impl, opj_stream_t **stream,
               opj_codec_t **codec, nrt_Error *error)
{
    if (nrt_IOInterface_seek(impl->io, impl->ioOffset, NRT_SEEK_SET, error) < 0)
    {
        goto CATCH_ERROR;
    }

    if (!(*stream = OpenJPEG_createIO(impl->io, 0, 1, error)))
    {
        goto CATCH_ERROR;
    }

    if (!(*codec = opj_create_decompress(CODEC_J2K)))
    {
        goto CATCH_ERROR;
    }

    opj_set_default_decoder_parameters(&impl->parameters);

    if (!opj_setup_decoder(*codec, &impl->parameters))
    {
        nrt_Error_init(error, "Error setting up openjpeg decoder", NRT_CTXT,
                       NRT_ERR_UNK);
        goto CATCH_ERROR;
    }

    return NRT_SUCCESS;

    CATCH_ERROR:
    {
        OpenJPEG_cleanup(stream, codec, NULL);
        return NRT_FAILURE;
    }
}

J2KPRIV( NRT_BOOL)
OpenJPEG_readHeader(OpenJPEGReaderImpl *impl, nrt_Error *error)
{
    OpenJPEGContainerImpl *container = NULL;
    opj_stream_t *stream = NULL;
    opj_image_t *image = NULL;
    opj_codec_t *codec = NULL;
    NRT_BOOL rc = NRT_SUCCESS;

    container = (OpenJPEGContainerImpl*)impl->container->data;

    if (!OpenJPEG_setup(impl, &stream, &codec, error))
    {
        goto CATCH_ERROR;
    }

    if (!opj_read_header(codec, &image, &container->x0, &container->y0,
                         &container->tileWidth, &container->tileHeight,
                         &container->xTiles, &container->yTiles, stream))
    {
        nrt_Error_init(error, "Error reading header", NRT_CTXT, NRT_ERR_UNK);
        goto CATCH_ERROR;
    }

    /* sanity checking */
    if (!image)
    {
        nrt_Error_init(error, "NULL image after reading header", NRT_CTXT,
                       NRT_ERR_UNK);
        goto CATCH_ERROR;
    }

    if (image->x1 - image->x0 <= 0 || image->y1 - image->y0 <= 0)
    {
        nrt_Error_init(error, "Invalid image offsets", NRT_CTXT, NRT_ERR_UNK);
        goto CATCH_ERROR;
    }
    if (image->numcomps == 0)
    {
        nrt_Error_init(error, "No image components found", NRT_CTXT,
                       NRT_ERR_UNK);
        goto CATCH_ERROR;
    }

    container->inputComponentBits = image->comps[0].prec;

    if (container->inputComponentBits > 16)
        container->componentBytes = 4;
    else if (container->inputComponentBits > 8)
        container->componentBytes = 2;
    else
        container->componentBytes = 1;

    container->width = image->x1 - image->x0;
    container->height = image->y1 - image->y0;
    container->nComponents = image->numcomps;

    goto CLEANUP;

    CATCH_ERROR:
    {
        rc = NRT_FAILURE;
    }

    CLEANUP:
    {
        OpenJPEG_cleanup(&stream, &codec, &image);
    }
    return rc;
}

J2KPRIV( NRT_BOOL)
OpenJPEG_initImage(OpenJPEGWriterImpl *impl, nrt_Error *error)
{
    NRT_BOOL rc = NRT_SUCCESS;
    nrt_Uint32 i, nComponents, height, width, tileHeight, tileWidth;
    nrt_Uint32 nBytes, nBits ,xTiles, yTiles, nTiles;
    nrt_Uint32 tileBytes;
    size_t uncompressedSize;
    int imageType;
    J2K_BOOL isSigned;
    opj_cparameters_t encoderParams;
    opj_image_cmptparm_t *cmptParams;
    OPJ_COLOR_SPACE colorSpace;

    nComponents = j2k_Container_getNumComponents(impl->container, error);
    width = j2k_Container_getWidth(impl->container, error);
    height = j2k_Container_getHeight(impl->container, error);
    tileWidth = j2k_Container_getTileWidth(impl->container, error);
    tileHeight = j2k_Container_getTileHeight(impl->container, error);
    xTiles = j2k_Container_getTilesX(impl->container, error);
    yTiles = j2k_Container_getTilesY(impl->container, error);
    nBytes = j2k_Container_getComponentBytes(impl->container, error);
    nBits = j2k_Container_getComponentBits(impl->container, error);
    isSigned = j2k_Container_isSigned(impl->container, error);
    imageType = j2k_Container_getImageType(impl->container, error);

    nTiles = xTiles * yTiles;
    tileBytes = tileWidth * tileHeight * nComponents * nBytes;
    uncompressedSize = tileBytes * nTiles;

    /* this is not ideal, but there is really no other way */
    if (!(impl->compressedBuf = (char*)J2K_MALLOC(uncompressedSize)))
    {
        nrt_Error_init(error, NRT_STRERROR(NRT_ERRNO), NRT_CTXT,
                       NRT_ERR_MEMORY);
        goto CATCH_ERROR;
    }
    if (!(impl->compressed = nrt_BufferAdapter_construct(impl->compressedBuf,
                                                         uncompressedSize, 1,
                                                         error)))
    {
        goto CATCH_ERROR;
    }

    if (!(impl->stream = OpenJPEG_createIO(impl->compressed, 0, 0, error)))
    {
        goto CATCH_ERROR;
    }

    /* setup the encoder parameters */
    /* TODO allow overrides somehow? */
    opj_set_default_encoder_parameters(&encoderParams);
    encoderParams.cp_disto_alloc = 1;
    encoderParams.tcp_numlayers = 1;
    encoderParams.tcp_rates[0] = 4.0;
    encoderParams.numresolution = 6;
    encoderParams.prog_order = LRCP; /* the default */
    encoderParams.cp_tx0 = 0;
    encoderParams.cp_ty0 = 0;
    encoderParams.tile_size_on = 1;
    encoderParams.cp_tdx = tileWidth;
    encoderParams.cp_tdy = tileHeight;
    encoderParams.irreversible = 0;

    /* TODO set error handler */

    if (!(cmptParams = (opj_image_cmptparm_t*)J2K_MALLOC(sizeof(
            opj_image_cmptparm_t) * nComponents)))
    {
        nrt_Error_init(error, NRT_STRERROR(NRT_ERRNO), NRT_CTXT,
                       NRT_ERR_MEMORY);
        goto CATCH_ERROR;
    }
    memset(cmptParams, 0, sizeof(opj_image_cmptparm_t) * nComponents);

    for(i = 0; i < nComponents; ++i)
    {
        cmptParams[i].x0 = 0;
        cmptParams[i].y0 = 0;
        cmptParams[i].dx = 1;
        cmptParams[i].dy = 1;
        cmptParams[i].w = width;
        cmptParams[i].h = height;
        cmptParams[i].prec = nBits;
        cmptParams[i].sgnd = isSigned;
    }

    switch(imageType)
    {
    case J2K_TYPE_RGB:
        colorSpace = CLRSPC_SRGB;
        break;
    default:
        colorSpace = CLRSPC_GRAY;
    }

    if (!(impl->codec = opj_create_compress(CODEC_J2K)))
    {
        nrt_Error_init(error, "Error creating OpenJPEG codec", NRT_CTXT,
                       NRT_ERR_INVALID_OBJECT);
        goto CATCH_ERROR;
    }
    if (!(impl->image = opj_image_tile_create(nComponents, cmptParams,
                                              colorSpace)))
    {
        nrt_Error_init(error, "Error creating OpenJPEG image", NRT_CTXT,
                       NRT_ERR_INVALID_OBJECT);
        goto CATCH_ERROR;
    }

    /* for some reason we must also explicitly specify these in the image... */
    impl->image->numcomps = nComponents;
    impl->image->x0 = 0;
    impl->image->y0 = 0;
    impl->image->x1 = width;
    impl->image->y1 = height;
    impl->image->color_space = colorSpace;

    if (!opj_setup_encoder(impl->codec, &encoderParams, impl->image))
    {
        nrt_Error_init(error, "Error setting up OpenJPEG decoder", NRT_CTXT,
                       NRT_ERR_INVALID_OBJECT);
        goto CATCH_ERROR;
    }

    if (!opj_start_compress(impl->codec, impl->image, impl->stream))
    {
        nrt_Error_init(error, "Error starting OpenJPEG compression", NRT_CTXT,
                       NRT_ERR_INVALID_OBJECT);
        goto CATCH_ERROR;
    }

    goto CLEANUP;

    CATCH_ERROR:
    {
        rc = NRT_FAILURE;
    }

    CLEANUP:
    {
        if (cmptParams)
            J2K_FREE(cmptParams);
    }

    return rc;
}


/******************************************************************************/
/* CONTAINER                                                                  */
/******************************************************************************/

J2KPRIV( nrt_Uint32)
OpenJPEGContainer_getTilesX(J2K_USER_DATA *data, nrt_Error *error)
{
    OpenJPEGContainerImpl *impl = (OpenJPEGContainerImpl*) data;
    return impl->xTiles;
}

J2KPRIV( nrt_Uint32)
OpenJPEGContainer_getTilesY(J2K_USER_DATA *data, nrt_Error *error)
{
    OpenJPEGContainerImpl *impl = (OpenJPEGContainerImpl*) data;
    return impl->yTiles;
}

J2KPRIV( nrt_Uint32)
OpenJPEGContainer_getTileWidth(J2K_USER_DATA *data, nrt_Error *error)
{
    OpenJPEGContainerImpl *impl = (OpenJPEGContainerImpl*) data;
    return impl->tileWidth;
}

J2KPRIV( nrt_Uint32)
OpenJPEGContainer_getTileHeight(J2K_USER_DATA *data, nrt_Error *error)
{
    OpenJPEGContainerImpl *impl = (OpenJPEGContainerImpl*) data;
    return impl->tileHeight;
}

J2KPRIV( nrt_Uint32)
OpenJPEGContainer_getWidth(J2K_USER_DATA *data, nrt_Error *error)
{
    OpenJPEGContainerImpl *impl = (OpenJPEGContainerImpl*) data;
    return impl->width;
}

J2KPRIV( nrt_Uint32)
OpenJPEGContainer_getHeight(J2K_USER_DATA *data, nrt_Error *error)
{
    OpenJPEGContainerImpl *impl = (OpenJPEGContainerImpl*) data;
    return impl->height;
}

J2KPRIV( nrt_Uint32)
OpenJPEGContainer_getNumComponents(J2K_USER_DATA *data, nrt_Error *error)
{
    OpenJPEGContainerImpl *impl = (OpenJPEGContainerImpl*) data;
    return impl->nComponents;
}

J2KPRIV( nrt_Uint32)
OpenJPEGContainer_getComponentBytes(J2K_USER_DATA *data, nrt_Error *error)
{
    OpenJPEGContainerImpl *impl = (OpenJPEGContainerImpl*) data;
    return impl->componentBytes;
}

J2KPRIV( nrt_Uint32)
OpenJPEGContainer_getComponentBits(J2K_USER_DATA *data, nrt_Error *error)
{
    OpenJPEGContainerImpl *impl = (OpenJPEGContainerImpl*) data;
    return impl->inputComponentBits;
}

J2KPRIV( int)
OpenJPEGContainer_getImageType(J2K_USER_DATA *data, nrt_Error *error)
{
    OpenJPEGContainerImpl *impl = (OpenJPEGContainerImpl*) data;
    return impl->imageType;
}

J2KPRIV( NRT_BOOL)
OpenJPEGContainer_isSigned(J2K_USER_DATA *data, nrt_Error *error)
{
    OpenJPEGContainerImpl *impl = (OpenJPEGContainerImpl*) data;
    return impl->isSigned;
}

J2KPRIV(void)
OpenJPEGContainer_destruct(J2K_USER_DATA * data)
{
    if (data)
    {
        OpenJPEGContainerImpl *impl = (OpenJPEGContainerImpl*) data;
        J2K_FREE(data);
    }
}

/******************************************************************************/
/* READER                                                                     */
/******************************************************************************/

J2KPRIV( NRT_BOOL)
OpenJPEGReader_canReadTiles(J2K_USER_DATA *data, nrt_Error *error)
{
    return NRT_SUCCESS;
}

J2KPRIV( nrt_Uint64)
OpenJPEGReader_readTile(J2K_USER_DATA *data, nrt_Uint32 tileX, nrt_Uint32 tileY,
                  nrt_Uint8 **buf, nrt_Error *error)
{
    OpenJPEGReaderImpl *impl = (OpenJPEGReaderImpl*) data;
    OpenJPEGContainerImpl *container = NULL;

    opj_stream_t *stream = NULL;
    opj_image_t *image = NULL;
    opj_codec_t *codec = NULL;
    nrt_Uint32 bufSize;

    if (!OpenJPEG_setup(impl, &stream, &codec, error))
    {
        goto CATCH_ERROR;
    }

    container = (OpenJPEGContainerImpl*)impl->container->data;

    /* unfortunately, we need to read the header every time ... */
    if (!opj_read_header(codec, &image, &container->x0, &container->y0,
                         &container->tileWidth, &container->tileHeight,
                         &container->xTiles, &container->yTiles, stream))
    {
        nrt_Error_init(error, "Error reading header", NRT_CTXT, NRT_ERR_UNK);
        goto CATCH_ERROR;
    }

    /* only decode what we want */
    if (!opj_set_decode_area(codec, container->tileWidth * tileX,
                             container->tileHeight * tileY,
                             container->tileWidth * (tileX + 1),
                             container->tileHeight * (tileY + 1)))
    {
        nrt_Error_init(error, "Error decoding area", NRT_CTXT, NRT_ERR_UNK);
        goto CATCH_ERROR;
    }

    {
        int keepGoing;
        OPJ_UINT32 tileIndex, nComponents;
        OPJ_INT32 tileX0, tileY0, tileX1, tileY1;

        if (!opj_read_tile_header(codec, &tileIndex, &bufSize, &tileX0,
                                  &tileY0, &tileX1, &tileY1, &nComponents,
                                  &keepGoing, stream))
        {
            nrt_Error_init(error, "Error reading tile header", NRT_CTXT,
                           NRT_ERR_UNK);
            goto CATCH_ERROR;
        }

        if (keepGoing)
        {

            if (buf && !*buf)
            {
                *buf = (nrt_Uint8*)J2K_MALLOC(bufSize);
                if (!*buf)
                {
                    nrt_Error_init(error, NRT_STRERROR(NRT_ERRNO), NRT_CTXT,
                                   NRT_ERR_MEMORY);
                    goto CATCH_ERROR;
                }
            }

            if (!opj_decode_tile_data(codec, tileIndex, *buf, bufSize, stream))
            {
                nrt_Error_init(error, "Error decoding tile", NRT_CTXT,
                               NRT_ERR_UNK);
                goto CATCH_ERROR;
            }
        }
    }

    goto CLEANUP;

    CATCH_ERROR:
    {
        bufSize = 0;
    }

    CLEANUP:
    {
        OpenJPEG_cleanup(&stream, &codec, &image);
    }
    return (nrt_Uint64)bufSize;
}

J2KPRIV( nrt_Uint64)
OpenJPEGReader_readRegion(J2K_USER_DATA *data, nrt_Uint32 x0, nrt_Uint32 y0,
                          nrt_Uint32 x1, nrt_Uint32 y1, nrt_Uint8 **buf,
                          nrt_Error *error)
{
    OpenJPEGReaderImpl *impl = (OpenJPEGReaderImpl*) data;
    OpenJPEGContainerImpl *container = NULL;

    opj_stream_t *stream = NULL;
    opj_image_t *image = NULL;
    opj_codec_t *codec = NULL;
    nrt_Uint64 bufSize;
    nrt_Uint64 offset = 0;

    if (!OpenJPEG_setup(impl, &stream, &codec, error))
    {
        goto CATCH_ERROR;
    }

    container = (OpenJPEGContainerImpl*)impl->container->data;

    /* unfortunately, we need to read the header every time ... */
    if (!opj_read_header(codec, &image, &container->x0, &container->y0,
                         &container->tileWidth, &container->tileHeight,
                         &container->xTiles, &container->yTiles, stream))
    {
        nrt_Error_init(error, "Error reading header", NRT_CTXT, NRT_ERR_UNK);
        goto CATCH_ERROR;
    }

    if (x1 == 0)
        x1 = container->width;
    if (y1 == 0)
        y1 = container->height;

    /* only decode what we want */
    if (!opj_set_decode_area(codec, x0, y0, x1, y1))
    {
        nrt_Error_init(error, "Error decoding area", NRT_CTXT, NRT_ERR_UNK);
        goto CATCH_ERROR;
    }

    bufSize = (nrt_Uint64)(x1 - x0) * (y1 - y0) * container->componentBytes
            * container->nComponents;
    if (buf && !*buf)
    {
        *buf = (nrt_Uint8*)J2K_MALLOC(bufSize);
        if (!*buf)
        {
            nrt_Error_init(error, NRT_STRERROR(NRT_ERRNO), NRT_CTXT,
                           NRT_ERR_MEMORY);
            goto CATCH_ERROR;
        }
    }

    {
        int keepGoing;
        OPJ_UINT32 tileIndex, nComponents, reqSize;
        OPJ_INT32 tileX0, tileY0, tileX1, tileY1;

        do
        {
            if (!opj_read_tile_header(codec, &tileIndex, &reqSize, &tileX0,
                                      &tileY0, &tileX1, &tileY1, &nComponents,
                                      &keepGoing, stream))
            {
                nrt_Error_init(error, "Error reading tile header", NRT_CTXT,
                               NRT_ERR_UNK);
                goto CATCH_ERROR;
            }

            if (keepGoing)
            {
                if (!opj_decode_tile_data(codec, tileIndex, (*buf + offset),
                                          reqSize, stream))
                {
                    nrt_Error_init(error, "Error decoding tile", NRT_CTXT,
                                   NRT_ERR_UNK);
                    goto CATCH_ERROR;
                }
                offset += reqSize;
            }
        }
        while (keepGoing);
    }

    goto CLEANUP;

    CATCH_ERROR:
    {
        bufSize = 0;
    }

    CLEANUP:
    {
        OpenJPEG_cleanup(&stream, &codec, &image);
    }
    return bufSize;
}

J2KPRIV( j2k_Container*)
OpenJPEGReader_getContainer(J2K_USER_DATA *data, nrt_Error *error)
{
    OpenJPEGReaderImpl *impl = (OpenJPEGReaderImpl*) data;
    return impl->container;
}

J2KPRIV(void)
OpenJPEGReader_destruct(J2K_USER_DATA * data)
{
    if (data)
    {
        OpenJPEGReaderImpl *impl = (OpenJPEGReaderImpl*) data;
        if (impl->io && impl->ownIO)
        {
            nrt_IOInterface_destruct(&impl->io);
            impl->io = NULL;
        }
        J2K_FREE(data);
    }
}

/******************************************************************************/
/* WRITER                                                                     */
/******************************************************************************/

J2KPRIV( NRT_BOOL)
OpenJPEGWriter_setTile(J2K_USER_DATA *data, nrt_Uint32 tileX, nrt_Uint32 tileY,
                       nrt_Uint8 *buf, nrt_Uint32 tileSize, nrt_Error *error)
{
    OpenJPEGWriterImpl *impl = (OpenJPEGWriterImpl*) data;
    NRT_BOOL rc = NRT_SUCCESS;
    nrt_Uint32 xTiles, yTiles, nTiles, tileIndex;

    xTiles = j2k_Container_getTilesX(impl->container, error);
    yTiles = j2k_Container_getTilesY(impl->container, error);
    tileIndex = tileY * xTiles + tileX;

    if (!opj_write_tile(impl->codec, tileIndex, buf, tileSize, impl->stream))
    {
        nrt_Error_init(error, "Error writing tile", NRT_CTXT,
                       NRT_ERR_INVALID_OBJECT);
        goto CATCH_ERROR;
    }

    goto CLEANUP;

    CATCH_ERROR:
    {
        rc = NRT_FAILURE;
    }

    CLEANUP:
    {
    }

    return rc;
}

J2KPRIV( NRT_BOOL)
OpenJPEGWriter_write(J2K_USER_DATA *data, nrt_IOInterface *io, nrt_Error *error)
{
    OpenJPEGWriterImpl *impl = (OpenJPEGWriterImpl*) data;
    NRT_BOOL rc = NRT_SUCCESS;
    size_t compressedSize;

    if (!opj_end_compress(impl->codec, impl->stream))
    {
        nrt_Error_init(error, "Error ending compression", NRT_CTXT,
                       NRT_ERR_INVALID_OBJECT);
        goto CATCH_ERROR;
    }

    /* just copy/write the compressed data to the output IO */
    compressedSize = (size_t)nrt_IOInterface_tell(impl->compressed, error);
    if (!nrt_IOInterface_write(io, impl->compressedBuf, compressedSize, error))
    {
        nrt_Error_init(error, "Error writing data", NRT_CTXT,
                       NRT_ERR_INVALID_OBJECT);
        goto CATCH_ERROR;
    }

    goto CLEANUP;

    CATCH_ERROR:
    {
        rc = NRT_FAILURE;
    }

    CLEANUP:
    {
    }

    return rc;
}

J2KPRIV( j2k_Container*)
OpenJPEGWriter_getContainer(J2K_USER_DATA *data, nrt_Error *error)
{
    OpenJPEGWriterImpl *impl = (OpenJPEGWriterImpl*) data;
    return impl->container;
}

J2KPRIV(void)
OpenJPEGWriter_destruct(J2K_USER_DATA * data)
{
    OpenJPEGWriterImpl *impl = (OpenJPEGWriterImpl*) data;
    if (data)
    {
        OpenJPEG_cleanup(&impl->stream, &impl->codec, &impl->image);
        NRT_FREE(data);
    }
}

/******************************************************************************/
/******************************************************************************/
/* PUBLIC FUNCTIONS                                                           */
/******************************************************************************/
/******************************************************************************/

J2KAPI(j2k_Reader*) j2k_Reader_open(const char *fname, nrt_Error *error)
{
    j2k_Reader *reader = NULL;
    nrt_IOHandle handle;
    nrt_IOInterface *io = NULL;

    if (!fname)
    {
        nrt_Error_init(error, "NULL filename", NRT_CTXT,
                       NRT_ERR_INVALID_OBJECT);
        goto CATCH_ERROR;
    }

    handle = nrt_IOHandle_create(fname, NRT_ACCESS_READONLY,
                                 NRT_OPEN_EXISTING, error);
    if (NRT_INVALID_HANDLE(handle))
    {
        nrt_Error_init(error, "Invalid IO handle", NRT_CTXT,
                               NRT_ERR_INVALID_OBJECT);
        goto CATCH_ERROR;
    }

    if (!(io = nrt_IOHandleAdapter_construct(handle, error)))
        goto CATCH_ERROR;

    if (!(reader = j2k_Reader_openIO(io, error)))
        goto CATCH_ERROR;

    ((OpenJPEGReaderImpl*) reader->data)->ownIO = 1;

    return reader;

    CATCH_ERROR:
    {
        if (io)
            nrt_IOInterface_destruct(&io);
        if (reader)
            j2k_Reader_destruct(&reader);
        return NULL;
    }
}

J2KAPI(j2k_Reader*) j2k_Reader_openIO(nrt_IOInterface *io, nrt_Error *error)
{
    OpenJPEGReaderImpl *impl = NULL;
    j2k_Reader *reader = NULL;
    OpenJPEGContainerImpl *container = NULL;

    /* create the Reader interface */
    impl = (OpenJPEGReaderImpl *) J2K_MALLOC(sizeof(OpenJPEGReaderImpl));
    if (!impl)
    {
        nrt_Error_init(error, NRT_STRERROR(NRT_ERRNO), NRT_CTXT, NRT_ERR_MEMORY);
        goto CATCH_ERROR;
    }
    memset(impl, 0, sizeof(OpenJPEGReaderImpl));

    reader = (j2k_Reader *) J2K_MALLOC(sizeof(j2k_Reader));
    if (!reader)
    {
        nrt_Error_init(error, NRT_STRERROR(NRT_ERRNO), NRT_CTXT, NRT_ERR_MEMORY);
        goto CATCH_ERROR;
    }
    memset(reader, 0, sizeof(j2k_Reader));
    reader->data = impl;
    reader->iface = &ReaderInterface;

    /* create the Container interface */
    container = (OpenJPEGContainerImpl *) J2K_MALLOC(sizeof(OpenJPEGContainerImpl));
    if (!container)
    {
        nrt_Error_init(error, NRT_STRERROR(NRT_ERRNO), NRT_CTXT, NRT_ERR_MEMORY);
        goto CATCH_ERROR;
    }
    memset(container, 0, sizeof(OpenJPEGContainerImpl));

    impl->container = (j2k_Container*) J2K_MALLOC(sizeof(j2k_Container));
    if (!impl->container)
    {
        nrt_Error_init(error, NRT_STRERROR(NRT_ERRNO), NRT_CTXT, NRT_ERR_MEMORY);
        goto CATCH_ERROR;
    }
    memset(impl->container, 0, sizeof(j2k_Container));
    impl->container->iface = &ContainerInterface;
    impl->container->data = container;

    /* initialize the interfaces */
    impl->io = io;
    impl->ioOffset = nrt_IOInterface_tell(io, error);

    if (!OpenJPEG_readHeader(impl, error))
    {
        goto CATCH_ERROR;
    }

    return reader;

    CATCH_ERROR:
    {
        if (reader)
        {
            j2k_Reader_destruct(&reader);
        }
        else if (impl)
        {
            OpenJPEGReader_destruct((J2K_USER_DATA*) impl);
        }
        return NULL;
    }
}

J2KAPI(j2k_Container*) j2k_Container_construct(nrt_Uint32 width,
        nrt_Uint32 height,
        nrt_Uint32 bands,
        nrt_Uint32 actualBitsPerPixel,
        nrt_Uint32 tileWidth,
        nrt_Uint32 tileHeight,
        int imageType,
        int isSigned,
        nrt_Error *error)
{
    j2k_Container *container = NULL;
    OpenJPEGContainerImpl *impl = NULL;

    container = (j2k_Container*) J2K_MALLOC(sizeof(j2k_Container));
    if (!container)
    {
        nrt_Error_init(error, NRT_STRERROR(NRT_ERRNO), NRT_CTXT, NRT_ERR_MEMORY);
        goto CATCH_ERROR;
    }
    memset(container, 0, sizeof(j2k_Container));

    /* create the Container interface */
    impl = (OpenJPEGContainerImpl *) J2K_MALLOC(sizeof(OpenJPEGContainerImpl));
    if (!impl)
    {
        nrt_Error_init(error, NRT_STRERROR(NRT_ERRNO), NRT_CTXT, NRT_ERR_MEMORY);
        goto CATCH_ERROR;
    }
    memset(impl, 0, sizeof(OpenJPEGContainerImpl));
    container->data = impl;
    container->iface = &ContainerInterface;

    impl->width = width;
    impl->height = height;
    impl->nComponents = bands;
    impl->componentBytes = (actualBitsPerPixel - 1) / 8 + 1;
    impl->inputComponentBits = actualBitsPerPixel;
    impl->tileWidth = tileWidth;
    impl->tileHeight = tileHeight;
    impl->imageType = imageType;
    impl->isSigned = isSigned;
    impl->xTiles = width / tileWidth + (width % tileWidth == 0 ? 0 : 1);
    impl->yTiles = height / tileHeight + (height % tileHeight == 0 ? 0 : 1);

    return container;

    CATCH_ERROR:
    {
        if (container)
        {
            j2k_Container_destruct(&container);
        }
        return NULL;
    }
}


J2KAPI(j2k_Writer*) j2k_Writer_construct(j2k_Container *container,
        nrt_Error *error)
{
    j2k_Writer *writer = NULL;
    OpenJPEGWriterImpl *impl = NULL;

    writer = (j2k_Writer*) J2K_MALLOC(sizeof(j2k_Container));
    if (!writer)
    {
        nrt_Error_init(error, NRT_STRERROR(NRT_ERRNO), NRT_CTXT, NRT_ERR_MEMORY);
        goto CATCH_ERROR;
    }
    memset(writer, 0, sizeof(j2k_Writer));

    /* create the Writer interface */
    impl = (OpenJPEGWriterImpl *) J2K_MALLOC(sizeof(OpenJPEGWriterImpl));
    if (!impl)
    {
        nrt_Error_init(error, NRT_STRERROR(NRT_ERRNO), NRT_CTXT, NRT_ERR_MEMORY);
        goto CATCH_ERROR;
    }
    memset(impl, 0, sizeof(OpenJPEGWriterImpl));
    impl->container = container;

    if (!(OpenJPEG_initImage(impl, error)))
    {
        goto CATCH_ERROR;
    }

    writer->data = impl;
    writer->iface = &WriterInterface;

    return writer;

    CATCH_ERROR:
    {
        if (writer)
        {
            j2k_Writer_destruct(&writer);
        }
        return NULL;
    }
}

#endif
