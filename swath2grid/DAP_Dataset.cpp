/******************************************************************************
 * $Id: TRMM_Dataset.cpp 2011-07-19 16:24:00Z $
 *
 * Project:  The Open Geospatial Consortium (OGC) Web Coverage Service (WCS)
 * 			 for Earth Observation: Open Source Reference Implementation
 * Purpose:  DAP_Dataset implementation for NOAA GOES data
 * Author:   Yuanzheng Shao, yshao3@gmu.edu
 *
 ******************************************************************************
 * Copyright (c) 2011, Liping Di <ldi@gmu.edu>, Yuanzheng Shao <yshao3@gmu.edu>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include <cstdlib>

#define DODS_DEBUG

#include "DAP_Dataset.h"

#include "Array.h"
#include "Grid.h"
#include "Float64.h"

#include "ce_functions.h"
#include "util.h"
#include "debug.h"

using namespace libdap;
using namespace std;

#define GOES_TIME_DEBUG FALSE

namespace libdap {

DAP_Dataset::DAP_Dataset()
{
}

/************************************************************************/
/*                           ~DAP_Dataset()                             */
/************************************************************************/

/**
 * \brief Destroy an open DAP_Dataset object.
 *
 * This is the accepted method of closing a DAP_Dataset dataset and
 * deallocating all resources associated with it.
 */

DAP_Dataset::~DAP_Dataset()
{
}

/************************************************************************/
/*                           DAP_Dataset()                              */
/************************************************************************/

/**
 * \brief Create an DAP_Dataset object.
 *
 * This is the accepted method of creating a DAP_Dataset object and
 * allocating all resources associated with it.
 *
 * @param id The coverage identifier.
 *
 * @param rBandList The field list selected for this coverage. For TRMM
 * daily data, the user could specify multiple days range in request.
 * Each day is seemed as one field.
 *
 * @return A DAP_Dataset object.
 */

DAP_Dataset::DAP_Dataset(const string& id, vector<int> &rBandList) :
        AbstractDataset(id, rBandList)
{
    md_MissingValue = 0;
    mb_GeoTransformSet = FALSE;
}

/**
 * @brief Initialize a DAP Dataset using Array objects already read.
 *
 *
 */

DAP_Dataset::DAP_Dataset(Array *src, Array *lat, Array *lon) :
        AbstractDataset(), m_src(src), m_lat(lat), m_lon(lon)
{
    // Read this from the 'missing_value' or '_FillValue' attributes
    string missing_value = m_src->get_attr_table().get_attr("missing_value");
    if (missing_value.empty())
        missing_value = m_src->get_attr_table().get_attr("_FillValue");

    if (!missing_value.empty())
        md_MissingValue = atof(missing_value.c_str());
    else
        md_MissingValue = 0;

    mb_GeoTransformSet = FALSE;
}

/************************************************************************/
/*                           InitialDataset()                           */
/************************************************************************/

/**
 * \brief Initialize the GOES dataset with NetCDF format.

 * This method is the implementation for initializing a GOES dataset with NetCDF format.
 * Within this method, SetNativeCRS(), SetGeoTransform() and SetGDALDataset()
 * will be called to initialize an GOES dataset.
 *
 * @note To use this, call this method and then access the GDALDataset that
 * contains the reprojected array using maptr_DS.get().
 *
 * @param isSimple the WCS request type.  When user executing a DescribeCoverage
 * request, isSimple is set to 1, and for GetCoverage, is set to 0.
 *
 * @return CE_None on success or CE_Failure on failure.
 */

CPLErr DAP_Dataset::InitialDataset(const int isSimple)
{
    GDALAllRegister();
    OGRRegisterAll();

    DBG(cerr << "In InitialDataset" << endl);

    // Might break that operation out so the remap is a separate call
    if (CE_None != SetNativeCRS() || CE_None != SetGeoTransform())
        throw Error("Could not set the dataset native CRS or the GeoTransform.");

    DBG(cerr << "Before SetGDALDataset" << endl);

    if (CE_None != SetGDALDataset(isSimple)) {
        GDALClose(maptr_DS.release());
        throw Error("Could not reproject the dataset.");
    }

    return CE_None;
}

/************************************************************************/
/*                            GetDAPArray()                            */
/************************************************************************/

/**
 * @brief Build a DAP Array from the GDALDataset
 */
Array *DAP_Dataset::GetDAPArray()
{
    DBG(cerr << "In GetDAPArray" << endl);
    DBG(cerr << "maptr_DS: " << maptr_DS.get() << endl);
    DBG(cerr << "raster band count: " << maptr_DS->GetRasterCount() << endl);

    // There should be just one band
    if (maptr_DS->GetRasterCount() != 1)
        throw Error("In function swath2grid(), expected a single raster band.");

    // Get the x and y dimensions of the raster band
    int x = maptr_DS->GetRasterXSize();
    int y = maptr_DS->GetRasterYSize();
    GDALRasterBand *rb = maptr_DS->GetRasterBand(1);
    if (!rb)
        throw Error("In function swath2grid(), could not access the raster data.");

    // Since the DAP_Dataset code works with all data values as doubles,
    // Assume the raster band has GDAL type GDT_Float64, but test anyway
    if (GDT_Float64 != rb->GetRasterDataType())
        throw Error("In function swath2grid(), expected raster data to be of type double.");

    DBG(cerr << "Destination array will have dimensions: " << x << ", " << y << endl);

    Array *a = new Array(m_src->name(), new Float64(m_src->name()));

    // Make the result array have two dimensions
    Array::Dim_iter i = m_src->dim_begin();

    a->append_dim(x, m_src->dimension_name(i));
    ++i;

    if (i == m_src->dim_end())
        throw Error("In function swath2grid(), expected source array to have two dimensions (2).");

    a->append_dim(y, m_src->dimension_name(i));

    // Poke in the data values
    /* RasterIO (   GDALRWFlag  eRWFlag,
    int     nXOff,
    int     nYOff,
    int     nXSize,
    int     nYSize,
    void *  pData,
    int     nBufXSize,
    int     nBufYSize,
    GDALDataType    eBufType,
    int     nPixelSpace,
    int     nLineSpace
    ) */
    vector<double> data(x * y);
    rb->RasterIO(GF_Read, 0, 0, x, y, &data[0], x, y, GDT_Float64, 0, 0);

    // NB: set_value() copies into new storage
    a->set_value(data, data.size());

    // Now poke in some attributes
    // TODO Make these CF attributes
    string projection_info = maptr_DS->GetProjectionRef();
    string gcp_projection_info = maptr_DS->GetGCPProjection();
    double geo_transform_coef[6];
    if (CE_None != maptr_DS->GetGeoTransform (geo_transform_coef))
        throw Error("In function swath2grid(), could not access the geo transform data.");

    DBG(cerr << "projection_info: " << projection_info << endl);
    DBG(cerr << "gcp_projection_info: " << gcp_projection_info << endl);
    DBG(cerr << "geo_transform coefs: " << double_to_string(geo_transform_coef[0]) << endl);

    AttrTable &attr = a->get_attr_table();
    attr.append_attr("projection", "String", projection_info);
    attr.append_attr("gcp_projection", "String", gcp_projection_info);
    for (int i = 0; i < sizeof(geo_transform_coef); ++i) {
        attr.append_attr("geo_transform_coefs", "String", double_to_string(geo_transform_coef[i]));
    }

    return a;
}

/************************************************************************/
/*                            GetDAPGrid()                              */
/************************************************************************/

/**
 * @brief Build a DAP Grid from the GDALDataset
 */
Grid *DAP_Dataset::GetDAPGrid()
{
    DBG(cerr << "In GetDAPGrid" << endl);

    Array *a = GetDAPArray();
    Array::Dim_iter i = a->dim_begin();
    int lon_size = a->dimension_size(i);
    int lat_size = a->dimension_size(++i);

    Grid *g = new Grid(a->name());
    g->add_var_nocopy(a, array);

    // Add maps; assume lon, lat; only two dimensions
    Array *lon = new Array("longtude", new Float64("longitude"));
    lon->append_dim(lon_size);
    vector<double> lon_data(lon_size);
    // Compute values
    // u = a*x + b*y
    // v = c*x + d*y
    // u,v --> x,y --> lon,lat
    // By the way, the values of the constants a, b, c, d are given by the 1, 2, 4, and 5
    // entries in the geotransform array.

    for (int j = 0; j < lon_size; ++j) {
        // lon_data[j] =
    }
    // load values
    lon->set_value(&lon_data[0], lon_size);
    return g;
}

/************************************************************************/
/*                            SetNativeCRS()                            */
/************************************************************************/

/**
 * \brief Set the Native CRS for a GOES dataset.
 *
 * The method will set the CRS for a GOES dataset as an native CRS.
 *
 * Since the original GOES data adopt satellite CRS to recored its value,
 * like MODIS swath data, each data point has its corresponding latitude
 * and longitude value, those coordinates could be fetched in another two fields.
 *
 * The native CRS for GOES Imager and Sounder data is assigned to EPSG:4326 if
 * both the latitude and longitude are existed.
 *
 * @return CE_None on success or CE_Failure on failure.
 */

CPLErr DAP_Dataset::SetNativeCRS()
{
    DBG(cerr << "In SetNativeCRS" << endl);

    mo_NativeCRS.SetWellKnownGeogCS("WGS84");

    return CE_None;
}

/************************************************************************/
/*                           SetGeoTransform()                          */
/************************************************************************/

/**
 * \brief Set the affine GeoTransform matrix for a GOES data.
 *
 * The method will set a GeoTransform matrix for a GOES data
 * by parsing the coordinates values existed in longitude and latitude field.
 *
 * The CRS for the bounding box is EPSG:4326.
 *
 * @return CE_None on success or CE_Failure on failure.
 */

CPLErr DAP_Dataset::SetGeoTransform()
{
    DBG(cerr << "In SetGeoTransform" << endl);

    // Assume the array is two dimensional
    Array::Dim_iter i = m_src->dim_begin();
    int nXSize = m_src->dimension_size(i, true);
    int nYSize = m_src->dimension_size(i + 1, true);

    mi_SrcImageXSize = nXSize;
    mi_SrcImageYSize = nYSize;

    SetGeoBBoxAndGCPs(nXSize, nYSize);

    double resX, resY;
    if (mdSrcGeoMinX > mdSrcGeoMaxX && mdSrcGeoMinX > 0 && mdSrcGeoMaxX < 0)
        resX = (360 + mdSrcGeoMaxX - mdSrcGeoMinX) / (nXSize - 1);
    else
        resX = (mdSrcGeoMaxX - mdSrcGeoMinX) / (nXSize - 1);

    resY = (mdSrcGeoMaxY - mdSrcGeoMinY) / (nYSize - 1);

    double res = MIN(resX, resY);

    if (mdSrcGeoMinX > mdSrcGeoMaxX && mdSrcGeoMinX > 0 && mdSrcGeoMaxX < 0)
        mi_RectifiedImageXSize = (int) ((360 + mdSrcGeoMaxX - mdSrcGeoMinX) / res) + 1;
    else
        mi_RectifiedImageXSize = (int) ((mdSrcGeoMaxX - mdSrcGeoMinX) / res) + 1;

    mi_RectifiedImageYSize = (int) fabs((mdSrcGeoMaxY - mdSrcGeoMinY) / res) + 1;

    DBG(cerr << "Source image size: " << nXSize << ", " << nYSize << endl);
    DBG(cerr << "Rectified image size: " << mi_RectifiedImageXSize << ", " << mi_RectifiedImageYSize << endl);

    md_Geotransform[0] = mdSrcGeoMinX;
    md_Geotransform[1] = res;
    md_Geotransform[2] = 0;
    md_Geotransform[3] = mdSrcGeoMaxY;
    md_Geotransform[4] = 0;
    md_Geotransform[5] = -res;
    mb_GeoTransformSet = TRUE;

    return CE_None;
}

/************************************************************************/
/*                         SetGeoBBoxAndGCPs()                          */
/************************************************************************/

/**
 * \brief Set the native geographical bounding box and GCP array for a GOES data.
 *
 * The method will set the native geographical bounding box
 * by comparing the coordinates values existed in longitude and latitude field.
 *
 * @param poVDS The GDAL dataset returned by calling GDALOpen() method.
 *
 * @return CE_None on success or CE_Failure on failure.
 */

void DAP_Dataset::SetGeoBBoxAndGCPs(int nXSize, int nYSize)
{
    DBG(cerr << "In SetGeoBBoxAndGCPs" << endl);

    // reuse the Dim_iter for both lat and lon arrays
    Array::Dim_iter i = m_lat->dim_begin();
    int nLatXSize = m_lat->dimension_size(i, true);
    int nLatYSize = m_lat->dimension_size(i + 1, true);
    i = m_lon->dim_begin();
    int nLonXSize = m_lon->dimension_size(i, true);
    int nLonYSize = m_lon->dimension_size(i + 1, true);

    if (nXSize != nLatXSize || nLatXSize != nLonXSize || nYSize != nLatYSize || nLatYSize != nLonYSize)
        throw Error("The size of latitude/longitude and data field does not match.");
#if 0
    /*
     *	Re-sample Standards:
     *	Height | Width
     *	(0, 500)		every other one pixel
     *	[500, 1000)		every other two pixels
     *	[1000,1500)		every other three pixels
     *	[1500,2000)		every other four pixels
     *	... ...
     */

    int xSpace = 1;
    int ySpace = 1;
    //setResampleStandard(poVDS, xSpace, ySpace);

    // TODO understand how GMU picked this value.
    // xSpace and ySpace are the stride values for sampling in
    // the x and y dimensions.
    const int RESAMPLE_STANDARD = 500;

    xSpace = int(nXSize / RESAMPLE_STANDARD) + 2;
    ySpace = int(nYSize / RESAMPLE_STANDARD) + 2;
#endif
    int nGCPs = 0;
    GDAL_GCP gdalCGP;

    m_lat->read();
    m_lon->read();
    double *dataLat = extract_double_array(m_lat);
    double *dataLon = extract_double_array(m_lon);

    DBG(cerr << "Past lat/lon data read" << endl);

    try {

        mdSrcGeoMinX = 360;
        mdSrcGeoMaxX = -360;
        mdSrcGeoMinY = 90;
        mdSrcGeoMaxY = -90;

        // Sample every fourth row and column
        int xSpace = 4;
        int ySpace = 4;

        for (int iLine = 0; iLine < nYSize - ySpace; iLine += ySpace) {
            for (int iPixel = 0; iPixel < nXSize - xSpace; iPixel += xSpace) {
                double x = *(dataLon + (iLine * nYSize) + iPixel);
                double y = *(dataLat + (iLine * nYSize) + iPixel);

                if (isValidLongitude(x) && isValidLatitude(y)) {
                    char pChr[64];
                    snprintf(pChr, 64, "%d", ++nGCPs);
                    GDALInitGCPs(1, &gdalCGP);
                    gdalCGP.pszId = strdup(pChr);
                    gdalCGP.pszInfo = strdup("");
                    gdalCGP.dfGCPLine = iLine;
                    gdalCGP.dfGCPPixel = iPixel;
                    gdalCGP.dfGCPX = x;
                    gdalCGP.dfGCPY = y;
                    gdalCGP.dfGCPZ = 0;
                    m_gdalGCPs.push_back(gdalCGP);

                    mdSrcGeoMinX = MIN(mdSrcGeoMinX, gdalCGP.dfGCPX);
                    mdSrcGeoMaxX = MAX(mdSrcGeoMaxX, gdalCGP.dfGCPX);
                    mdSrcGeoMinY = MIN(mdSrcGeoMinY, gdalCGP.dfGCPY);
                    mdSrcGeoMaxY = MAX(mdSrcGeoMaxY, gdalCGP.dfGCPY);
                }
            }
        }
    }
    catch (...) {
        delete[] dataLat;
        delete[] dataLon;
        throw;
    }

    delete[] dataLat;
    delete[] dataLon;

    DBG(cerr << "Leaving SetGeoBBoxAndGCPs" << endl);
}

/************************************************************************/
/*                           SetGDALDataset()                           */
/************************************************************************/

/**
 * \brief Make a 'memory' dataset with one band
 *
 * @return CE_None on success or CE_Failure on failure.
 */

CPLErr DAP_Dataset::SetGDALDataset(const int isSimple)
{
    DBG(cerr << "In SetGDALDataset" << endl);

    // NB: mi_RectifiedImageXSize & Y are set in SetGeoTransform()
    GDALDataType eBandType = GDT_Float64;
    GDALDriverH poDriver = GDALGetDriverByName("MEM");
    GDALDataset* satDataSet = (GDALDataset*) GDALCreate(poDriver, "", mi_RectifiedImageXSize, mi_RectifiedImageYSize,
            1, eBandType, NULL);
    if (NULL == satDataSet) {
        GDALClose(poDriver);
        throw Error("Failed to create \"MEM\" dataSet.");
    }

    GDALRasterBand *poBand = satDataSet->GetRasterBand(1);
    poBand->SetNoDataValue(md_MissingValue);

    m_src->read();
    double *data = extract_double_array(m_src);
    if (CE_None != poBand->RasterIO(GF_Write, 0, 0, mi_RectifiedImageXSize, mi_RectifiedImageYSize, data,
                    mi_SrcImageXSize, mi_SrcImageYSize, eBandType, 0, 0)) {
        GDALClose((GDALDatasetH) satDataSet);
        throw Error("Failed to satellite data band to VRT DataSet.");
    }
    delete[] data;

    //set GCPs for this VRTDataset
    if (CE_None != SetGCPGeoRef4VRTDataset(satDataSet)) {
        GDALClose((GDALDatasetH) satDataSet);
        throw Error("Could not georeference the virtual dataset.");
    }

    DBG(cerr << "satDataSet: " << satDataSet << endl);

    maptr_DS.reset(satDataSet);

    if (isSimple)
        return CE_None;

    return RectifyGOESDataSet();
}

/************************************************************************/
/*                       SetGCPGeoRef4VRTDataset()                      */
/************************************************************************/

/**
 * \brief Set the GCP array for the VRT dataset.
 *
 * This method is used to set the GCP array to created VRT dataset based on GDAL
 * method SetGCPs().
 *
 * @param poVDS The VRT dataset.
 *
 * @return CE_None on success or CE_Failure on failure.
 */

CPLErr DAP_Dataset::SetGCPGeoRef4VRTDataset(GDALDataset* poVDS)
{
    char* psTargetSRS;
    mo_NativeCRS.exportToWkt(&psTargetSRS);

#if (__GNUC__ >=4 && __GNUC_MINOR__ > 1)
    if (CE_None != poVDS->SetGCPs(m_gdalGCPs.size(), (GDAL_GCP*) (m_gdalGCPs.data()), psTargetSRS)) {
        OGRFree(psTargetSRS);
        throw Error("Failed to set GCPs.");
    }
#else
    {
        if(CE_None!=poVDS->SetGCPs(m_gdalGCPs.size(), (GDAL_GCP*)&m_gdalGCPs[0], psTargetSRS))
        {
            OGRFree( psTargetSRS );
            throw Error("Failed to set GCPs.");
        }
    }
#endif

    OGRFree(psTargetSRS);

    return CE_None;
}

/************************************************************************/
/*                        SetMetaDataList()                             */
/************************************************************************/

/**
 * \brief Set the metadata list for this coverage.
 *
 * The method will set the metadata list for the coverage based on its
 * corresponding GDALDataset object.
 *
 * @param hSrc the GDALDataset object corresponding to coverage.
 *
 * @return CE_None on success or CE_Failure on failure.
 */

CPLErr DAP_Dataset::SetMetaDataList(GDALDataset* hSrcDS)
{
    // TODO Remove
#if 0
    mv_MetaDataList.push_back("Product_Description=The data was created by GMU WCS from NOAA GOES satellite data.");
    mv_MetaDataList.push_back("unit=GVAR");
    mv_MetaDataList.push_back("FillValue=0");
    ms_FieldQuantityDef = "GVAR";
    ms_AllowRanges = "0 65535";
    ms_CoveragePlatform = "GOES-11";
    ms_CoverageInstrument = "GOES-11";
    ms_CoverageSensor = "Imager";
#endif

    return CE_None;
}

/************************************************************************/
/*                          GetGeoMinMax()                              */
/************************************************************************/

/**
 * \brief Get the min/max coordinates of laitutude and longitude.
 *
 * The method will fetch the min/max coordinates of laitutude and longitude.
 *
 * @param geoMinMax an existing four double buffer into which the
 * native geographical bounding box values will be placed.
 *
 * @return CE_None on success or CE_Failure on failure.
 */

CPLErr DAP_Dataset::GetGeoMinMax(double geoMinMax[])
{
    if (!mb_GeoTransformSet)
        return CE_Failure;

    geoMinMax[0] = mdSrcGeoMinX;
    geoMinMax[2] = mdSrcGeoMinY;
    geoMinMax[1] = mdSrcGeoMaxX;
    geoMinMax[3] = mdSrcGeoMaxY;

    return CE_None;
}

/************************************************************************/
/*                          RectifyGOESDataSet()                        */
/************************************************************************/

/**
 * \brief Convert the GOES dataset from satellite CRS project to grid CRS.
 *
 * The method will convert the GOES dataset from satellite CRS project to
 * grid CRS based on GDAL API GDALReprojectImage;
 *
 * @return CE_None on success or CE_Failure on failure.
 */

CPLErr DAP_Dataset::RectifyGOESDataSet()
{
    DBG(cerr << "In RectifyGOESDataSet" << endl);

    char *pszDstWKT;
    mo_NativeCRS.exportToWkt(&pszDstWKT);

    GDALDriverH poDriver = GDALGetDriverByName("MEM");
    GDALDataset* rectDataSet = (GDALDataset*) GDALCreate(poDriver, "", mi_RectifiedImageXSize, mi_RectifiedImageYSize,
            maptr_DS->GetRasterCount(), maptr_DS->GetRasterBand(1)->GetRasterDataType(), NULL);
    if (NULL == rectDataSet) {
        GDALClose(poDriver);
        OGRFree(pszDstWKT);
        throw Error("Failed to create \"MEM\" dataSet.");
    }

    rectDataSet->SetProjection(pszDstWKT);
    rectDataSet->SetGeoTransform(md_Geotransform);

    DBG(cerr << "rectDataSet: " << rectDataSet << endl);
    DBG(cerr << "satDataSet: " << maptr_DS.get() << endl);

    // FIXME Magic value of 0.125
    if (CE_None != GDALReprojectImage(maptr_DS.get(), NULL, rectDataSet, pszDstWKT,
            GRA_NearestNeighbour, 0, 0.125, NULL, NULL, NULL)) {
        GDALClose(rectDataSet);
        GDALClose(poDriver);
        OGRFree(pszDstWKT);
        throw Error("Failed to re-project GOES data from satellite GCP CRS to geographical CRS.");
    }

    OGRFree(pszDstWKT);
    GDALClose(maptr_DS.release());

    maptr_DS.reset(rectDataSet);

    DBG(cerr << "Leaving RectifyGOESDataSet" << endl);

    return CE_None;
}

} // namespace libdap
