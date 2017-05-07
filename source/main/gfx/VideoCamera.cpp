/*
    This source file is part of Rigs of Rods
    Copyright 2005-2012 Pierre-Michel Ricordel
    Copyright 2007-2012 Thomas Fischer
    Copyright 2016-2017 Petr Ohlidal & contributors

    For more information, see http://www.rigsofrods.org/

    Rigs of Rods is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 3, as
    published by the Free Software Foundation.

    Rigs of Rods is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Rigs of Rods. If not, see <http://www.gnu.org/licenses/>.
*/

#include "VideoCamera.h"

#include "Application.h"
#include "Beam.h"
#include "BeamData.h"
#include "GUIManager.h"
#include "OgreSubsystem.h"
#include "RigDef_File.h"
#include "RigSpawner.h"
#include "RoRFrameListener.h"
#include "Settings.h"
#include "SkyManager.h"
#include "Utils.h"

using namespace Ogre;
using namespace RoR;

int VideoCamera::counter = 0;

VideoCamera::VideoCamera(rig_t* truck) :
    truck(truck)
    , debugMode(false)
    , debugNode(0)
    , mVidCam()
    , mat()
    , rttTex(0)
    , rwMirror(0)
{
    debugMode = App::GetDiagVideoCameras();
}

void VideoCamera::init()
{
    mat = RoR::OgreSubsystem::GetMaterialByName(materialName);

    mVidCam = gEnv->sceneManager->createCamera(materialName + "_camera");

    bool useExternalMirrorWindow = BSETTING("UseVideocameraWindows", false);
    bool fullscreenRW = BSETTING("VideoCameraFullscreen", false);

    // check if this vidcamera is also affected
    if (useExternalMirrorWindow && fullscreenRW)
    {
        int monitor = ISETTING("VideoCameraMonitor_" + TOSTRING(counter), 0);
        if (monitor < 0)
            useExternalMirrorWindow = false;
        // < 0 = fallback to texture
    }

    if (!useExternalMirrorWindow)
    {
        Ogre::TexturePtr rttTexPtr = Ogre::TextureManager::getSingleton().createManual(materialName + "_texture"
            , Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME
            , Ogre::TEX_TYPE_2D
            , mirrorSize.x
            , mirrorSize.y
            , 0 // no mip maps
            , Ogre::PF_R8G8B8
            , Ogre::TU_RENDERTARGET);
        rttTex = rttTexPtr->getBuffer()->getRenderTarget();
        rttTex->setAutoUpdated(false);
    }
    else
    {
        NameValuePairList misc;
        if (!SSETTING("VideoCameraFSAA", "").empty())
            misc["FSAA"] = SSETTING("VideoCameraFSAA", "");

        if (!SSETTING("VideoCameraColourDepth", "").empty())
            misc["colourDepth"] = SSETTING("VideoCameraColourDepth", "");
        else
            misc["colourDepth"] = "32";

        if (ISETTING("VideoCameraLeft_" + TOSTRING(counter), 0) > 0)
            misc["left"] = SSETTING("VideoCameraLeft_" + TOSTRING(counter), "");

        if (ISETTING("VideoCameraTop_" + TOSTRING(counter), 0) > 0)
            misc["top"] = SSETTING("VideoCameraTop_" + TOSTRING(counter), "");
        if (!SSETTING("VideoCameraWindowBorder", "").empty())
            misc["border"] = SSETTING("VideoCameraWindowBorder", ""); // fixes for windowed mode

        misc["outerDimensions"] = "true"; // fixes for windowed mode

        bool fullscreen = BSETTING("VideoCameraFullscreen", false);
        if (fullscreen)
        {
            int monitor = ISETTING("VideoCameraMonitor_" + TOSTRING(counter), 0);
            misc["monitorIndex"] = TOSTRING(monitor);
        }

        rwMirror = Ogre::Root::getSingleton().createRenderWindow(vidCamName, mirrorSize.x, mirrorSize.y, fullscreen, &misc);
        if (ISETTING("VideoCameraLeft_" + TOSTRING(counter), 0) > 0)
            rwMirror->reposition(ISETTING("VideoCameraLeft_" + TOSTRING(counter), 0), ISETTING("VideoCameraTop_" + TOSTRING(counter), 0));

        if (ISETTING("VideoCameraWidth_" + TOSTRING(counter), 0) > 0)
            rwMirror->resize(ISETTING("VideoCameraWidth_" + TOSTRING(counter), 0), ISETTING("VideoCameraHeight_" + TOSTRING(counter), 0));

        rwMirror->setAutoUpdated(false);
        fixRenderWindowIcon(rwMirror);
        rwMirror->setDeactivateOnFocusChange(false);
        // TODO: disable texture mirrors
    }

    mVidCam->setNearClipDistance(minclip);
    mVidCam->setFarClipDistance(maxclip);
    mVidCam->setFOVy(Ogre::Degree(fov));
    mVidCam->setAspectRatio((float)mirrorSize.x / (float)mirrorSize.y);

    disabledTexture = mat->getTechnique(0)->getPass(0)->getTextureUnitState(0)->getTextureName();
    mat->getTechnique(0)->getPass(0)->setLightingEnabled(false);

    if (rttTex)
    {
        Ogre::Viewport* vp = rttTex->addViewport(mVidCam);
        vp->setClearEveryFrame(true);
        vp->setBackgroundColour(gEnv->mainCamera->getViewport()->getBackgroundColour());
        vp->setVisibilityMask(~HIDE_MIRROR);
        vp->setVisibilityMask(~DEPTHMAP_DISABLED);
        vp->setOverlaysEnabled(false);

        mat->getTechnique(0)->getPass(0)->getTextureUnitState(0)->setTextureName(materialName + "_texture");

        // this is a mirror, flip the image left<>right to have a mirror and not a cameraimage
        if (camRole == 1)
            mat->getTechnique(0)->getPass(0)->getTextureUnitState(0)->setTextureUScale(-1);
    }

    if (rwMirror)
    {
        Ogre::Viewport* vp = rwMirror->addViewport(mVidCam);
        vp->setClearEveryFrame(true);
        vp->setBackgroundColour(gEnv->mainCamera->getViewport()->getBackgroundColour());
        vp->setVisibilityMask(~HIDE_MIRROR);
        vp->setVisibilityMask(~DEPTHMAP_DISABLED);
        vp->setOverlaysEnabled(false);
        mat->getTechnique(0)->getPass(0)->getTextureUnitState(0)->setTextureName(disabledTexture);
    }

    if (debugMode)
    {
        Ogre::ManualObject* mo = this->CreateVideocameraDebugMesh();
        debugNode = gEnv->sceneManager->getRootSceneNode()->createChildSceneNode();
        debugNode->attachObject(mo);
    }
}

void VideoCamera::setActive(bool state)
{
    if (rttTex)
    {
        rttTex->setActive(state);
        if (state)
            mat->getTechnique(0)->getPass(0)->getTextureUnitState(0)->setTextureName(materialName + "_texture");
        else
            mat->getTechnique(0)->getPass(0)->getTextureUnitState(0)->setTextureName(disabledTexture);
    }

    if (rwMirror)
        rwMirror->setActive(state);
}

void VideoCamera::update(float dt)
{
    if (RoR::App::GetActiveSimState() == RoR::App::SIM_STATE_PAUSED) { return; } // Do nothing when paused

#ifdef USE_CAELUM
    // caelum needs to know that we changed the cameras
    if (gEnv->sky && RoR::App::GetActiveAppState() == RoR::App::APP_STATE_SIMULATION)
    {
        gEnv->sky->notifyCameraChanged(mVidCam);
    }

#endif // USE_CAELUM

    // update the texture now, otherwise shuttering
    if (rttTex)
        rttTex->update();

    if (rwMirror)
        rwMirror->update();

    // get the normal of the camera plane now
    Vector3 normal = (-(truck->nodes[nref].AbsPosition - truck->nodes[nz].AbsPosition)).crossProduct(-(truck->nodes[nref].AbsPosition - truck->nodes[ny].AbsPosition));
    normal.normalise();

    // add user set offset
    Vector3 pos = truck->nodes[camNode].AbsPosition +
        (offset.x * normal) +
        (offset.y * (truck->nodes[nref].AbsPosition - truck->nodes[ny].AbsPosition)) +
        (offset.z * (truck->nodes[nref].AbsPosition - truck->nodes[nz].AbsPosition));

    //avoid the camera roll
    // camup orientates to frustrum of world by default -> rotating the cam related to trucks yaw, lets bind cam rotation videocamera base (nref,ny,nz) as frustum
    // could this be done faster&better with a plane setFrustumExtents ?
    Vector3 frustumUP = truck->nodes[nref].AbsPosition - truck->nodes[ny].AbsPosition;
    frustumUP.normalise();
    mVidCam->setFixedYawAxis(true, frustumUP);

    // camRole 1 = mirror
    if (camRole == 1)
    {
        //rotate the normal of the mirror by user rotation setting so it reflects correct
        normal = rotation * normal;
        // merge camera direction and reflect it on our plane
        mVidCam->setDirection((pos - gEnv->mainCamera->getPosition()).reflect(normal));
    }
    else
    {
        // this is a videocamera
        if (camRole == -1)
        {
            // rotate the camera according to the nodes orientation and user rotation
            Vector3 refx = truck->nodes[nz].AbsPosition - truck->nodes[nref].AbsPosition;
            refx.normalise();
            Vector3 refy = truck->nodes[nref].AbsPosition - truck->nodes[ny].AbsPosition;
            refy.normalise();
            Quaternion rot = Quaternion(-refx, -refy, -normal);
            mVidCam->setOrientation(rot * rotation); // rotate the camera orientation towards the calculated cam direction plus user rotation
        }
        else
        {
            // we assume this is a tracking videocamera
            normal = truck->nodes[lookat].AbsPosition - pos;
            normal.normalise();
            Vector3 refx = truck->nodes[nz].AbsPosition - truck->nodes[nref].AbsPosition;
            refx.normalise();
            // why does this flip ~2-3° around zero orientation and only with trackercam. back to slower crossproduct calc, a bit slower but better .. sigh
            // Vector3 refy = truck->nodes[nref].AbsPosition - truck->nodes[ny].AbsPosition;
            Vector3 refy = refx.crossProduct(normal);
            refy.normalise();
            Quaternion rot = Quaternion(-refx, -refy, -normal);
            mVidCam->setOrientation(rot * rotation); // rotate the camera orientation towards the calculated cam direction plus user rotation
        }
    }

    if (debugMode)
    {
        debugNode->setPosition(pos);
        debugNode->setOrientation(mVidCam->getOrientation());
    }

    // set the new position
    mVidCam->setPosition(pos);
}

VideoCamera* VideoCamera::CreateVideoCamera(RigSpawner* rig_spawner, Ogre::MaterialPtr own_material, RigDef::VideoCamera& def)
{
    try
    {
        VideoCamera* v = new VideoCamera(rig_spawner->GetRig());
        v->fov = def.field_of_view;
        v->minclip = def.min_clip_distance;
        v->maxclip = def.max_clip_distance;
        v->nz = rig_spawner->GetNodeIndexOrThrow(def.left_node);
        v->ny = rig_spawner->GetNodeIndexOrThrow(def.bottom_node);
        v->nref = rig_spawner->GetNodeIndexOrThrow(def.reference_node);
        v->offset = def.offset;
        v->switchoff = def.camera_mode; // add performance switch off  ->meeds fix, only "always on" supported yet
        v->materialName = own_material->getName();
        v->mirrorSize = Vector2(def.texture_width, def.texture_height);

        /* camera name */
        if (! def.camera_name.empty())
        {
            v->vidCamName = def.camera_name;
        }
        else
        {
            v->vidCamName = def.material_name; /* Fallback */
        }

        //rotate camera picture 180�, skip for mirrors
        float rotation_z = (def.camera_role != 1) ? def.rotation.z + 180 : def.rotation.z;
        v->rotation
            = Ogre::Quaternion(Ogre::Degree(rotation_z), Ogre::Vector3::UNIT_Z)
            * Ogre::Quaternion(Ogre::Degree(def.rotation.y), Ogre::Vector3::UNIT_Y)
            * Ogre::Quaternion(Ogre::Degree(def.rotation.x), Ogre::Vector3::UNIT_X);

        // set alternative camposition (optional)
        v->camNode = rig_spawner->GetNodeIndexOrThrow(def.alt_reference_node.IsValidAnyState() ? def.alt_reference_node : def.reference_node);

        // set alternative lookat position (optional)
        int camera_role = def.camera_role;
        if (def.alt_orientation_node.IsValidAnyState())
        {
            v->lookat = rig_spawner->GetNodeIndexOrThrow(def.alt_orientation_node);
            camera_role = 0; // this is a tracecam, overwrite mode setting
        }
        else
        {
            v->lookat = -1;
        }
        v->camRole = camera_role; // -1= camera, 0 = trackcam, 1 = mirror

        v->init();

        return v;
    }
    catch (...)
    {
        return nullptr;
    }
}

Ogre::ManualObject* VideoCamera::CreateVideocameraDebugMesh()
{
    // Create material
    static size_t counter = 0;
    Ogre::MaterialPtr mat = Ogre::MaterialManager::getSingleton().create(
        "VideoCamDebugMat-" + TOSTRING(counter), Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
    ++counter;
    mat->getTechnique(0)->getPass(0)->createTextureUnitState();
    mat->getTechnique(0)->getPass(0)->getTextureUnitState(0)->setTextureFiltering(Ogre::TFO_ANISOTROPIC);
    mat->getTechnique(0)->getPass(0)->getTextureUnitState(0)->setTextureAnisotropy(3);
    mat->setLightingEnabled(false);
    mat->setReceiveShadows(false);
    // Create mesh
    Ogre::ManualObject* mo = gEnv->sceneManager->createManualObject(); // TODO: Eliminate gEnv
    mo->begin(mat->getName(), Ogre::RenderOperation::OT_LINE_LIST);
    Ogre::ColourValue pos_mark_col(1.f, 0.82f, 0.26f);
    Ogre::ColourValue dir_mark_col(0.f, 1.f, 1.f); // TODO: This comes out green in simulation - why? ~ only_a_ptr, 05/2017
    const float pos_mark_len = 0.8f;
    const float dir_mark_len = 4.f;
    // X
    mo->position(pos_mark_len,0,0);
    mo->colour(pos_mark_col);
    mo->position(-pos_mark_len,0,0);
    mo->colour(pos_mark_col);
    // Y
    mo->position(0,pos_mark_len,0);
    mo->colour(pos_mark_col);
    mo->position(0,-pos_mark_len,0);
    mo->colour(pos_mark_col);
    // +Z
    mo->position(0,0,pos_mark_len);
    mo->colour(pos_mark_col);
    mo->position(0,0,0);
    mo->colour(pos_mark_col);
    // -Z = the direction
    mo->position(0,0,-dir_mark_len);
    mo->colour(dir_mark_col);
    mo->position(0,0,0);
    mo->colour(dir_mark_col);
    mo->end(); // Don't forget this!

    return mo;
}
