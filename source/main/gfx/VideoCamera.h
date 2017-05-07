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

#pragma once

#include "RoRPrerequisites.h"
#include "RigDef_Prerequisites.h"

#include <OgreMaterial.h>

class VideoCamera : public ZeroedMemoryAllocator
{
public:
    VideoCamera(rig_t* truck);

    void init();

    void update(float dt);

    void setActive(bool state);

    /// Factory method
    static VideoCamera* CreateVideoCamera(RigSpawner* rig_spawner, Ogre::MaterialPtr own_material, RigDef::VideoCamera& def);

    int camNode, lookat, switchoff;
    float fov, minclip, maxclip;
    Ogre::Vector3 offset;
    Ogre::Vector2 mirrorSize;
    Ogre::Quaternion rotation;

    int nz, ny, nref, camRole;
    Ogre::String materialName, disabledTexture, vidCamName;

private:
    Ogre::ManualObject* CreateVideocameraDebugMesh();

    rig_t* truck;
    static int counter;
    Ogre::Camera* mVidCam;
    Ogre::RenderTexture* rttTex;
    Ogre::MaterialPtr mat;
    bool debugMode;
    Ogre::SceneNode* debugNode;
    Ogre::RenderWindow* rwMirror;
};
