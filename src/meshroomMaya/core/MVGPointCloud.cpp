#include "meshroomMaya/core/MVGPointCloud.hpp"
#include "meshroomMaya/core/MVGCamera.hpp"
#include "meshroomMaya/core/MVGLog.hpp"
#include "meshroomMaya/core/MVGProject.hpp"
#include "meshroomMaya/core/MVGGeometryUtil.hpp"
#include "meshroomMaya/core/MVGPlaneKernel.hpp"
#include "meshroomMaya/maya/MVGMayaUtil.hpp"
#include <maya/M3dView.h>
#include <maya/MFnParticleSystem.h>
#include <maya/MVectorArray.h>
#include <maya/MPointArray.h>
#include <maya/MDoubleArray.h>
#include <maya/MSelectionList.h>
#include <maya/MDagModifier.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnVectorArrayData.h>
#include <maya/MPlug.h>
#include <maya/MMatrix.h>
#include <stdexcept>

namespace meshroomMaya
{

namespace
{ // empty namespace

int isLeft(const MPoint& P0, const MPoint& P1, const MPoint& P2)
{
    // isLeft(): tests if a point is Left|On|Right of an infinite line.
    //    Input:  three points P0, P1, and P2
    //    Return: >0 for P2 left of the line through P0 and P1
    //            =0 for P2  on the line
    //            <0 for P2  right of the line
    //    See: Algorithm 1 "Area of Triangles and Polygons"
    return (int)((P1.x - P0.x) * (P2.y - P0.y) - (P2.x - P0.x) * (P1.y - P0.y));
}

int wn_PnPoly(const MPoint& P, const MPointArray& V)
{
    // wn_PnPoly(): winding number test for a point in a polygon
    //      Input:   P = a point,
    //               V[] = vertex points of a polygon V[n+1] with V[n]=V[0]
    //      Return:  wn = the winding number (=0 only when P is outside)
    int wn = 0;
    for(int i = 0; i < V.length() - 1; i++)
    {
        if(V[i].y <= P.y)
        {
            if(V[i + 1].y > P.y)
                if(isLeft(V[i], V[i + 1], P) > 0)
                    ++wn;
        }
        else
        {
            if(V[i + 1].y <= P.y)
                if(isLeft(V[i], V[i + 1], P) < 0)
                    --wn;
        }
    }
    return wn;
}

} // empty namespace

MVGPointCloud::MVGPointCloud(const std::string& name)
    : MVGNodeWrapper(name)
{
}

MVGPointCloud::MVGPointCloud(const MDagPath& dagPath)
    : MVGNodeWrapper(dagPath)
{
}

MVGPointCloud::~MVGPointCloud()
{
}

bool MVGPointCloud::isValid() const
{
    return _dagpath.isValid();
}

MStatus MVGPointCloud::getItems(std::vector<MVGPointCloudItem>& items) const
{
    MStatus status;
    items.clear();
    MFnParticleSystem fnParticle(_dagpath, &status);
    CHECK_RETURN_STATUS(status)
    MVectorArray positionArray;
    fnParticle.position(positionArray);
    items.resize(positionArray.length());
    for(int i = 0; i < positionArray.length(); ++i)
    {
        MVGPointCloudItem& item = items[i];
        item._id = i;
        item._position = positionArray[i];
    }
    return status;
}

MStatus MVGPointCloud::getItems(std::vector<MVGPointCloudItem>& items,
                                const MIntArray& indexes) const
{
    MStatus status;
    items.clear();
    items.reserve(indexes.length());
    MFnParticleSystem fnParticle(_dagpath, &status);
    CHECK_RETURN_STATUS(status)
    MVectorArray positionArray;
    fnParticle.position(positionArray);
    for(int i = 0; i < indexes.length(); ++i)
    {
        MVGPointCloudItem item;
        item._id = indexes[i];
        item._position = positionArray[indexes[i]];
        items.push_back(item);
    }
    return status;
}

/**
 *
 * @param[in] view
 * @param[in] visibleItems : pointcloud items visible for the current camera
 * @param[in] faceCSPoints : points describing the face in camera space coordinates
 * @param[out] faceWSPoints : faceCSPoints projected on computed plane in world space
 *coordinates
 * @return
 */
bool MVGPointCloud::projectPoints(M3dView& view, const std::vector<MVGPointCloudItem>& visibleItems,
                                  const MPointArray& faceCSPoints, MPointArray& faceWSPoints)
{
    if(!isValid())
        return false;
    if(faceCSPoints.length() < 3)
        return false;
    if(visibleItems.size() < 3)
        return false;

    MPointArray closedVSPolygon(MVGGeometryUtil::cameraToViewSpace(view, faceCSPoints));
    closedVSPolygon.append(closedVSPolygon[0]); // add an extra point (to describe a closed shape)

    // get enclosed items in pointcloud
    MPointArray enclosedWSPoints;
    std::vector<MVGPointCloudItem>::const_iterator it = visibleItems.begin();
    int windingNumber = 0;
    for(; it != visibleItems.end(); ++it)
    {
        windingNumber =
            wn_PnPoly(MVGGeometryUtil::worldToViewSpace(view, it->_position), closedVSPolygon);
        if(windingNumber != 0)
            enclosedWSPoints.append(it->_position);
    }
    if(enclosedWSPoints.length() < 3)
        return false;

    // Compute plane
    PlaneKernel::Model model;
    MVGGeometryUtil::computePlane(enclosedWSPoints, model);
    // Project points
    return MVGGeometryUtil::projectPointsOnPlane(view, faceCSPoints, model, faceWSPoints);
}

/**
 *
 * @param[in] view
 * @param[in] visibleItems : pointcloud items visible for the current camera
 * @param[in] faceCSPoints : points describing the face in camera space coordinates
 * @param[in] constraintedWSPoints : points describing the line constraint in world space
 *coordinates
 * @param[in] mouseCSPoint : mouse camera space coordinates
 * @param[out] projectedWSMouse : mouseCSPoint projected on computed plane in world space
 * @return
 */
bool MVGPointCloud::projectPointsWithLineConstraint(
    M3dView& view, const std::vector<MVGPointCloudItem>& visibleItems,
    const MPointArray& faceCSPoints, const MPointArray& constraintedWSPoints,
    const MPoint& mouseCSPoint, MPoint& projectedWSMouse)
{
    if(!isValid())
        return false;
    if(faceCSPoints.length() < 3)
        return false;
    if(visibleItems.size() < 3)
        return false;
    if(constraintedWSPoints.length() < 2)
        return false;

    MPointArray closedVSPolygon(MVGGeometryUtil::cameraToViewSpace(view, faceCSPoints));
    closedVSPolygon.append(closedVSPolygon[0]); // add an extra point (to describe a closed shape)

    // get enclosed items in pointcloud
    MPointArray enclosedWSPoints;
    std::vector<MVGPointCloudItem>::const_iterator it = visibleItems.begin();
    int windingNumber = 0;
    for(; it != visibleItems.end(); ++it)
    {
        windingNumber =
            wn_PnPoly(MVGGeometryUtil::worldToViewSpace(view, it->_position), closedVSPolygon);
        if(windingNumber != 0)
            enclosedWSPoints.append(it->_position);
    }
    if(enclosedWSPoints.length() < 3)
        return false;

    LineConstrainedPlaneKernel::Model model;
    MVGGeometryUtil::computePlaneWithLineConstraint(enclosedWSPoints, constraintedWSPoints, model);

    // Project the mouse point
    return MVGGeometryUtil::projectPointOnPlane(view, mouseCSPoint, model, projectedWSMouse);
}

MStatus MVGPointCloud::setOpacity(double value)
{
    MFnParticleSystem fn(_dagpath);
    MDoubleArray array(fn.count(), value);
    return setOpacityPPAttribute(array);
}

MStatus MVGPointCloud::setOpacity(const MIntArray &indices, double value)
{
    MDoubleArray array;
    getOpacityPP(array);
    for(unsigned int i=0; i<indices.length(); ++i)
        array[indices[i]] = value;
    return setOpacityPPAttribute(array);
}

MStatus MVGPointCloud::getOpacityPP(MDoubleArray& values)
{
    MStatus status;
    MFnParticleSystem fn(_dagpath, &status);
    ensureOpacityPPAttribute();
    fn.getPerParticleAttribute("opacityPP", values, &status);
    return status;
}

MStatus MVGPointCloud::setOpacityPPAttribute(MDoubleArray& values)
{
    MStatus status;
    MFnParticleSystem fn(_dagpath, &status);
    ensureOpacityPPAttribute();
    fn.setPerParticleAttribute("opacityPP", values, &status);
    return status;
}

MStatus MVGPointCloud::ensureOpacityPPAttribute()
{
    MStatus status;
    MFnParticleSystem fn(_dagpath, &status);
    if(!fn.isPerParticleDoubleAttribute("opacityPP"))
    {
        MFnTypedAttribute typedAttr;
        MObject attrObject = typedAttr.create("opacityPP", "opacityPP", MFnData::kDoubleArray);
        fn.addAttribute(attrObject);
    }
    return status;
}

} // namespace
