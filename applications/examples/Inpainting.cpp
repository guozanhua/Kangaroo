#include <pangolin/pangolin.h>
#include <pangolin/glcuda.h>

#include <kangaroo/kangaroo.h>
#include <kangaroo/variational.h>
#include <kangaroo/common/DisplayUtils.h>
#include <kangaroo/common/BaseDisplayCuda.h>
#include <kangaroo/common/ImageSelect.h>

using namespace std;
using namespace pangolin;

int main( int argc, char* argv[] )
{
    // Open video device
    const std::string vid_uri = argc >= 2 ? argv[1] : "";    
    pangolin::VideoInput video(vid_uri);
    if(video.PixFormat().format != "GRAY8")
        throw pangolin::VideoException("Wrong format. Gray8 required.");

    // Image dimensions and host copy
    const unsigned int w = video.Width();
    const unsigned int h = video.Height();
    Gpu::Image<unsigned char, Gpu::TargetHost, Gpu::Manage> host(w,h);

    // Initialise window
    View& container = SetupPangoGLWithCuda(2*w, 2*h,180);

    // Allocate Camera Images on device for processing
    Gpu::Image<unsigned char, Gpu::TargetDevice, Gpu::Manage> img(w,h);
    Gpu::Image<float, Gpu::TargetDevice, Gpu::Manage> imgg(w,h);
    Gpu::Image<float, Gpu::TargetDevice, Gpu::Manage> imgu(w,h);
    Gpu::Image<float2, Gpu::TargetDevice, Gpu::Manage> imgp(w,h);
    Gpu::Image<float, Gpu::TargetDevice, Gpu::Manage> imgdivp(w,h);
    Gpu::Image<float, Gpu::TargetDevice, Gpu::Manage> imglambda(w,h);

    const bool bilinear = false;
    ActivateDrawImage<float> adg(imgg, GL_LUMINANCE32F_ARB, bilinear, true);
    ActivateDrawImage<float> adu(imgu, GL_LUMINANCE32F_ARB, bilinear, true);
    ActivateDrawImage<float> addivp(imgdivp, GL_LUMINANCE32F_ARB, bilinear, true);
    ActivateDrawImage<float> adlambda(imglambda, GL_LUMINANCE32F_ARB, bilinear, true);

    Handler2dImageSelect handler2d(w,h);
    SetupContainer(container, 4, (float)w/h);
    container[0].SetDrawFunction(boost::ref(adg)).SetHandler(&handler2d);
    container[1].SetDrawFunction(boost::ref(adu)).SetHandler(&handler2d);
    container[2].SetDrawFunction(boost::ref(addivp)).SetHandler(&handler2d);
    container[3].SetDrawFunction(boost::ref(adlambda)).SetHandler(&handler2d);

    Var<bool> run("ui.run", true, true);
    Var<bool> step("ui.step", false, false);

    const float L = sqrt(8);
    Var<float> sigma("ui.sigma", 1.0f/L, 0, 0.1);
    Var<float> tau("ui.tau", 1.0f/L, 0, 0.1);
    Var<float> lambda("ui.lambda", 1.2, 0, 10);
    Var<float> alpha("ui.alpha", 0.002, 0, 0.005);

    Var<float> r("ui.r", 10, 1, 50);

    pangolin::RegisterKeyPressCallback(' ', [&run](){run = !run;} );
    pangolin::RegisterKeyPressCallback(PANGO_SPECIAL + GLUT_KEY_RIGHT, [&step](){step=true;} );

    for(unsigned long frame=0; !pangolin::ShouldQuit(); ++frame)
    {
        bool go = (frame==0) || Pushed(step);

        if(go) {
            if(video.GrabNext(host.ptr)) {
                img.CopyFrom(host);
                Gpu::ElementwiseScaleBias<float,unsigned char,float>(imgg, img, 1.0f/255.0f);
                imgu.CopyFrom(imgg);
                imgp.Memset(0);
                imgdivp.Memset(0);
                Gpu::Fill<float>(imglambda, 1.0);
            }
        }

        go |= run;
        if(go) {
            for(int i=0; i<10; ++i ) {
                Gpu::HuberGradU_DualAscentP(imgp,imgu,sigma,alpha);
                Gpu::Divergence(imgdivp,imgp);
                Gpu::L2_u_minus_g_PrimalDescent(imgu,imgp,imgg,imglambda, tau, lambda);
            }
        }

        if(handler2d.IsSelected()) {
            Eigen::Vector2d p = handler2d.GetSelectedPoint(true);
            Gpu::PaintCircle<float>(imglambda, 0.0f, p[0], p[1], r);
        }

        /////////////////////////////////////////////////////////////
        // Perform drawing
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glColor3f(1,1,1);

        pangolin::FinishGlutFrame();
    }
}