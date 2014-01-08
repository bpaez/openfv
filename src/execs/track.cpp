#include "std_include.h"

#include "calibration.h"
#include "refocusing.h"
#include "pLoc.h"
#include "tracking.h"
#include "tools.h"
#include "optimization.h"

#include "cuda_lib.h"

using namespace cv;
using namespace std;

int main(int argc, char** argv) {
    
    double rn = atof(argv[2]);
    double rs = atof(argv[3]);

    pTracking track(argv[1], rn, rs);
    track.track_frames(0, 2);
    //track.track_all();

    //track.write_quiver_data();

    track.plot_all_paths();
    //track.plot_complete_paths();
    //track.write_all_paths("../temp/track_movie_nn.txt");

    return 0;

}
