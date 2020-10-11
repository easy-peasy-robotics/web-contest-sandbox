// -*- mode:C++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-
//
// Author: Ugo Pattacini - <ugo.pattacini@iit.it>

#include <cstdlib>
#include <memory>
#include <string>

#include <yarp/os/ResourceFinder.h>
#include <yarp/os/RFModule.h>
#include <yarp/os/Value.h>
#include <yarp/os/Vocab.h>
#include <yarp/os/BufferedPort.h>
#include <yarp/os/RpcServer.h>
#include <yarp/os/Property.h>
#include <yarp/os/LogStream.h>

#include <yarp/dev/PolyDriver.h>
#include <yarp/dev/GazeControl.h>

#include <yarp/sig/Image.h>
#include <yarp/sig/PointCloud.h>
#include <yarp/sig/Vector.h>
#include <yarp/sig/Matrix.h>

#include <yarp/math/Math.h>

#include <yarp/cv/Cv.h>
#include <yarp/pcl/Pcl.h>

/*************************************************************************************/
class Module: public yarp::os::RFModule {
    bool simulation{true};

    yarp::dev::PolyDriver driverHead;
    yarp::dev::PolyDriver driverGaze;
    yarp::dev::IGazeControl* igaze;
    double fov_h{0.};

    yarp::os::BufferedPort<yarp::sig::ImageOf<yarp::sig::PixelRgb>> imgLPort;
    yarp::os::BufferedPort<yarp::sig::ImageOf<yarp::sig::PixelRgb>> imgRPort;
    yarp::os::BufferedPort<yarp::sig::ImageOf<yarp::sig::PixelFloat>> depthPort;
    yarp::os::RpcServer rpcPort;

    /*********************************************************************************/
    bool configure(yarp::os::ResourceFinder& rf) override {
        auto robot = rf.check("robot", yarp::os::Value("icubSim")).asString();
        simulation = (robot == "icubSim");

        yarp::os::Property optionsHead;
        optionsHead.put("device", "remote_controlboard");
        optionsHead.put("remote", "/" + robot + "/head");
        optionsHead.put("local", "/head");
        if (!driverHead.open(optionsHead)) {
            yError() << "Unable to connect to the robot head";
            return false;
        }

        yarp::os::Property optionsGaze;
        optionsGaze.put("device", "gazecontrollerclient");
        optionsGaze.put("remote", "/iKinGazeCtrl");
        optionsGaze.put("local", "/gaze");
        if (driverGaze.open(optionsGaze)) {
            driverGaze.view(igaze);
        } else {
            yError() << "Unable to connect to iKinGazeCtrl";
            driverHead.close();
            return false;
        }

        // get camera intrinsics
        yarp::os::Bottle info;
        igaze->getInfo(info);
        fov_h = info.find("camera_intrinsics_left").asList()->get(0).asDouble();

        imgLPort.open("/imgL:i");
        imgRPort.open("/imgR:i");
        depthPort.open("/depth:i");

        rpcPort.open("/service");
        attach(rpcPort);

        return true;
    }

    /*********************************************************************************/
    bool interruptModule() override {
        imgLPort.interrupt();
        imgRPort.interrupt();
        depthPort.interrupt();
        return true;
    }

    /*********************************************************************************/
    bool close() override {
        driverHead.close();
        driverGaze.close();
        imgLPort.close();
        imgRPort.close();
        depthPort.close();
        rpcPort.close();
        return true;
    }

    /*********************************************************************************/
    bool respond(const yarp::os::Bottle& cmd,
                 yarp::os::Bottle& reply) override {
        if (cmd.get(0).asVocab() == yarp::os::Vocab::encode("go")) {
            reply.addInt(go());
        } else {
            // the father class already handles the "quit" command
            return yarp::os::RFModule::respond(cmd, reply);
        }
        return true;
    }

    /*********************************************************************************/
    double getPeriod() override {
        return 0.;  // sync upon incoming images
    }

    /*********************************************************************************/
    auto get_pointcloud(const yarp::sig::ImageOf<yarp::sig::PixelRgb>& imgL,
                        const yarp::sig::ImageOf<yarp::sig::PixelFloat>& depth,
                        const yarp::sig::Matrix& Teye) const {
        // aggregate image data in the point cloud of the whole scene
        const auto w = depth.width();
        const auto h = depth.height();
        auto pc = std::make_shared<yarp::sig::PointCloud<yarp::sig::DataXYZRGBA>>();
        yarp::sig::Vector x{0., 0., 0., 1.};
        for (int v = 0; v < h; v++) {
            for (int u = 0; u < w; u++) {
                const auto rgb = imgL(u, v);
                const auto d = depth(u, v);
                
                if (d > 0.F) {
                    x[0] = d * (u - .5 * (w - 1)) / fov_h;
                    x[1] = d * (v - .5 * (h - 1)) / fov_h;
                    x[2] = d;
                    x = Teye * x;
                
                    pc->push_back(yarp::sig::DataXYZRGBA());
                    auto& p = (*pc)(pc->size() - 1);
                    p.x = (float)x[0];
                    p.y = (float)x[1];
                    p.z = (float)x[2];
                    p.r = rgb.r;
                    p.g = rgb.g;
                    p.b = rgb.b;
                    p.a = 255;
                }
            }
        }
        return pc;
    }

    /*********************************************************************************/
    bool updateModule() override {
        // get fresh images
        auto* imgL = imgLPort.read();
        auto* imgR = imgRPort.read();
        auto* depth = depthPort.read();

        // interrupt sequence detected
        if ((imgL == nullptr) || (imgR == nullptr) || (depth == nullptr)) {
            return false;
        }

        // get camera extrinsics
        // we need extrinsincs for expressing the point clouds
        // in the root frame rather than the eye frame,
        // so that we can then deal with the proper coordinates
        // system to finally determine the position of the object
        // on the table
        yarp::sig::Vector cam_x, cam_o;
        igaze->getLeftEyePose(cam_x, cam_o);
        auto Teye = yarp::math::axis2dcm(cam_o);
        Teye.setSubcol(cam_x, 0, 3);

        // FILL IN THE CODE

        // when you aim to get the point cloud of the scene
        // just call the following:
        // auto pc = get_pointcloud(*imgL, *depth, Teye);
        
        return true;
    }

    /*********************************************************************************/
    int go() {
        // FILL IN THE CODE

        // return here the position of the object on the table
        // shall be in {1, 2, 4, 5}
        return -1;
    }
};


/*************************************************************************************/
int main(int argc, char *argv[]) {
    yarp::os::Network yarp;
    if (!yarp.checkNetwork()) {
        yError() << "YARP doesn't seem to be available";
        return EXIT_FAILURE;
    }

    yarp::os::ResourceFinder rf;
    rf.configure(argc, argv);

    Module module;
    return module.runModule(rf);
}