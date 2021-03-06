#include "CDPR.h"
#include "json.hpp"
#include "Dependencies\eigen-3.3.7\Eigen\Dense"
#include <iostream>
#include <fstream>
#include <cmath>

using namespace Eigen;
using namespace std;
using json = nlohmann::json;

// Constructor, based on the requested model
CDPR::CDPR(){
    // Read json file into json object
    json model;
    try{
        ifstream file("model.json", ifstream::binary);
        file >> model;
        cout << "Imported model.json: " << model.value("ModelName", "NOT FOUND") << endl;
        
        // Initialize parameters
        this->nodeNum = model.value("tekMotorNum", 8); // Default value set in 2nd input
        this->absTorqueLmt = model.value("absTorqueLmt", 60);
        this->endEffOffset = model.value("endEffOffset", -0.28);
        this->targetTorque = model.value("targetTorque", -2.5);
        this->cmdScale = model.value("toMotorCmdScale", 509295);
        //this->railScale = model.value("railCmdScale", 38400000);
        this->pRadius = model.value("pulleyRadius", 0.045);
        // Interate through arrays
        for(int i=0; i<6; i++){
            this->home[i] = model["home"][i];
            this->limit[i] = (float) model["limit"][i];
        }
        for(int i=0; i<nodeNum; i++){
            frmOutUnitV[i] << 0, 0, model["pulleyZdir"][i];
            for(int j=0; j<3; j++){ // for each xyz coordinates
                this->frmOut[i][j] = model["frameAttachments"][i][j];
                this->endOut[i][j] = model["endEffectorAttachments"][i][j];
            }
        }
    }
    catch(const exception& e){
        cout << "Error in reading \"model.json\"" << endl;
        cout << e.what() << endl;
        isValidModel = false;
    }
}

bool CDPR::IsGood(){ return isValidModel; }
bool CDPR::CheckLimits(){
    for (int i = 0; i < 3; i++){
        if(limit[i*2]>in[i] || in[i]>limit[i*2+1]){ return false; }
    }
    return true;
}

void CDPR::PrintIn(){ // Print in[]
    cout << "IN: "<< in[0] << " " << in[1] << " " << in[2] << " " << in[3] << " " << in[4] << " " << in[5] << endl;
}

void CDPR::PrintOut(){ // Print out[]
    cout << "OUT: ";
    for(int i = 0; i < nodeNum; i++){ cout << out[i] << " "; }
    cout << endl;
}

void CDPR::PrintHome(){ // Print home[]
    cout << "HOME: "<< home[0] << " " << home[1] << " " << home[2] << " " << home[3] << " " << home[4] << " " << home[5] << endl;
}

void CDPR::PoseToLength(double pose[], double lengths[], double rail_offset){ // given an EE pose, calculate the EE to pulley cable lengths
    // local variables
    Vector3d orB[8]; // vector from frame 0 origin to end effector cable outlet
    Vector3d orA[8]; // vector from frame 0 origin to cable outlet point on rotating pulley
    Matrix3d oe_R; // rotation matrix from end effector to frame 0
    Matrix3d Ra, Rb, Rc;
    double a = pose[3], b = pose[4], c = pose[5];
    
    Ra << 1, 0, 0,
          0, cos(a), -sin(a),
          0, sin(a), cos(a);
    Rb << cos(b), 0,  sin(b),
          0, 1, 0,
          -sin(b), 0, cos(b);
    Rc << cos(c), -sin(c), 0, 
          sin(c), cos(c), 0, 
          0, 0, 1;
    oe_R = Ra * Rb * Rc; // Rotation matrix from given pose rotation
    Vector3d endEfr(pose[0], pose[1], pose[2]); // Translation of end-effector
    for(int i = 0; i < nodeNum; i++){
        ///// Calculate orB[8] vectors /////
        orB[i] = oe_R*endOut[i] + endEfr;
                
        ///// Calculate cable outlet point on rotating pulley, ie point A /////
        Vector3d plNormal = frmOutUnitV[i].cross(orB[i] - frmOut[i]); // normal vector of the plane that the rotating pulley is in
        Vector3d VecC = plNormal.cross(frmOutUnitV[i]); // direction from fixed point towards pulley center
        Vector3d orC = VecC/VecC.norm()*pRadius + frmOut[i]; // vector from frame 0 origin to rotating pulley center
        double triR = pRadius / (orB[i] - orC).norm(); // triangle ratio??
        orA[i] = orC + triR*triR*(orB[i] - orC) - triR*sqrt(1 - triR*triR)*((plNormal/plNormal.norm()).cross(orB[i] - orC));
        
        ///// Calculate arc length /////
        Vector3d UVecCF = -VecC / VecC.norm(); // unit vector of CF
        Vector3d UVecCA = (orA[i]- orC) / (orA[i]- orC).norm(); // unit vector of CA
        double l_arc = pRadius * acos(UVecCF.dot(UVecCA));

        ///// Sum the total cable length /////
        lengths[i] = i < 4 ? l_arc + (orA[i] - orB[i]).norm() : l_arc + (orA[i] - orB[i]).norm() - rail_offset; // Need to subtract the rail offset for the bottom 4 motors 
    }
}

double CDPR::EEOffset(){ return endEffOffset; }
float CDPR::TargetTorque(){ return targetTorque; }
float CDPR::AbsTorqLmt(){ return absTorqueLmt; }
int CDPR::NodeNum(){ return nodeNum; }
int32_t CDPR::MotorScale(){ return cmdScale; }
// int32_t CDPR::RailScale(){ return railScale; }

int32_t CDPR::ToMotorCmd(int motorID, double length){
    if(motorID == -1) { return length * cmdScale; }
    return (length - this->offset[motorID]) * cmdScale;
}