/*
 * Stellarium
 * This file Copyright (C) 2004 Robert Spearman
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <functional>
#include <QSettings>

#include "Projector.hpp"
#include "Navigator.hpp"
#include "MeteorMgr.hpp"
#include "StelApp.hpp"
#include "StelCore.hpp"
#include "Meteor.hpp"
#include "LandscapeMgr.hpp"
#include "StelModuleMgr.hpp"
#include "InitParser.hpp"

MeteorMgr::MeteorMgr(int zhr, int maxv )
{
	setObjectName("MeteorMgr");
			
	ZHR = zhr;
	max_velocity = maxv;

	// calculate factor for meteor creation rate per second since visible area ZHR is for
	// estimated visible radius of 458km
	// (calculated for average meteor magnitude of +2.5 and limiting magnitude of 5)

	//  zhr_to_wsr = 1.0f/3600.f;
	zhr_to_wsr = 1.6667f/3600.f;
	// this is a correction factor to adjust for the model as programmed to match observed rates
}

MeteorMgr::~MeteorMgr()
{
	for(vector<Meteor*>::iterator iter = active.begin(); iter != active.end(); ++iter)
	{
		delete *iter;
		active.erase(iter);
	}
}

void MeteorMgr::init()
{
	setZHR(StelApp::getInstance().getSettings()->value("astro/meteor_rate", 10).toInt());
}

/*************************************************************************
 Reimplementation of the getCallOrder method
*************************************************************************/
double MeteorMgr::getCallOrder(StelModuleActionName actionName) const
{
	if (actionName==StelModule::ACTION_DRAW)
		return StelApp::getInstance().getModuleMgr().getModule("SolarSystem")->getCallOrder(actionName)+10;
	return 0;
}

void MeteorMgr::setZHR(int zhr)
{
	ZHR = zhr;
}

int MeteorMgr::getZHR()
{
	return ZHR;
}

void MeteorMgr::setMaxVelocity(int maxv)
{
	max_velocity = maxv;
}

void MeteorMgr::update(double delta_time)
{
	delta_time*=1000;
	Projector * proj = StelApp::getInstance().getCore()->getProjection();
	Navigator * nav = StelApp::getInstance().getCore()->getNavigation();
	ToneReproducer * eye = StelApp::getInstance().getCore()->getToneReproducer();
	

	// step through and update all active meteors
	int n =0;
	for(vector<Meteor*>::iterator iter = active.begin(); iter != active.end(); ++iter)
	{
		n++;
		//printf("Meteor %d update\n", ++n);
		if( !( (*iter)->update(delta_time) ) )
		{
			// remove dead meteor
			//      printf("Meteor \tdied\n");

			delete *iter;
			active.erase(iter);
			iter--;  // important!
		}
	}

	// only makes sense given lifetimes of meteors to draw when time_speed is realtime
	// otherwise high overhead of large numbers of meteors
	double tspeed = nav->getTimeSpeed() *86400;  // sky seconds per actual second
	if(tspeed <= 0 || fabs(tspeed) > 1 )
	{
		// don't start any more meteors
		return;
	}

	/*
	// debug - one at a time
	if(active.begin() == active.end() ) {
	  Meteor *m = new Meteor(projection, navigation, max_velocity);
	  active.push_back(m);
	    }
	*/


	// if stellarium has been suspended, don't create huge number of meteors to
	// make up for lost time!
	if( delta_time > 500 )
	{
		delta_time = 500;
	}

	// determine average meteors per frame needing to be created
	int mpf = (int)((double)ZHR*zhr_to_wsr*(double)delta_time/1000.0f + 0.5);
	if( mpf < 1 ) mpf = 1;

	int mlaunch = 0;
	for(int i=0; i<mpf; i++)
	{

		// start new meteor based on ZHR time probability
		double prob = (double)rand()/((double)RAND_MAX+1);
		if( ZHR > 0 && prob < ((double)ZHR*zhr_to_wsr*(double)delta_time/1000.0f/(double)mpf) )
		{
			Meteor *m = new Meteor(proj, nav, eye, max_velocity);
			active.push_back(m);
			mlaunch++;
		}
	}

	//  printf("mpf: %d\tm launched: %d\t(mps: %f)\t%d\n", mpf, mlaunch, ZHR*zhr_to_wsr, delta_time);


}


double MeteorMgr::draw(StelCore* core)
{
	LandscapeMgr* landmgr = (LandscapeMgr*)StelApp::getInstance().getModuleMgr().getModule("LandscapeMgr");
	if (landmgr->getFlagAtmosphere() && landmgr->getLuminance()>5)
		return 0.;
	
	core->getProjection()->setCurrentFrame(Projector::FRAME_LOCAL);
	
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);
	glDisable(GL_TEXTURE_2D);  // much dimmer without this

	// step through and draw all active meteors
	for(vector<Meteor*>::iterator iter = active.begin(); iter != active.end(); ++iter)
	{
		(*iter)->draw(core->getProjection(), core->getNavigation());
	}

	glEnable(GL_TEXTURE_2D);
	
	return 0.0;  // TODO, actually calculate movement on screen

}
