// -------------------------------------------------------
// -------------------------------------------------------
// Synthetic Aperture - Particle Tracking Velocimetry Code
// --- 3D PIV Library ---
// -------------------------------------------------------
// Author: Abhishek Bajpayee
//         Dept. of Mechanical Engineering
//         Massachusetts Institute of Technology
// -------------------------------------------------------
// -------------------------------------------------------

#include "std_include.h"
#include "calibration.h"
#include "typedefs.h"
#include "piv.h"
// #include "cuda_lib.h"

//#include <fftw3.h>
#include "tools.h"

#include <cufftw.h>
#include <opencv2/opencv.hpp>
#include <opencv2/gpu/gpu.hpp>

using namespace std;
using namespace cv;

piv3D::piv3D(int zero_padding) {

    frames_ = 0;

    mean_shift_ = 0;
    zero_padding_ = zero_padding;

}

void piv3D::add_frame(vector<Mat> mats) {

    Mat3 vol(mats);
    frames.push_back(vol);
    if (frames_==0) {
        zs_ = mats.size();

        if (zs_ == 0)
            LOG(FATAL)<<"Empty volume!";

        xs_ = mats[0].rows;
        ys_ = mats[0].cols;
    }

    frames_++;

}

void piv3D::run(int l) {

    wx_ = l; wy_ = l; wz_ = l;

    double overlap = 0.5;
    vector< vector<int> > winx, winy, winz;
    winx = get_windows(xs_, wx_, overlap);
    winy = get_windows(ys_, wy_, overlap);
    winz = get_windows(zs_, wz_, overlap);

    if (zero_padding_) {
        wx_ *= 2; wy_ *= 2; wz_ *= 2;
    }

    double *i1, *i2, *i3;
    fftw_complex *o1;
    i1 = new double[wx_*wx_*wx_];
    i2 = new double[wy_*wy_*wy_];
    i3 = new double[wz_*wz_*wz_];
    o1 = new fftw_complex[wx_*wy_*(wz_/2+1)];

    // // initializing reference
    // for (int k = 0; k < l; k++) {
    //     for (int i = 0; i < l; i++) {
    //         for (int j = 0; j < l; j++) {
    //             int ind = k+l*(j+l*i);
    //             i2[ind] = i+j+k+5;
    //         }
    //     }
    // }

    double s = omp_get_wtime();
    
    fileIO file("../../velcity.txt");

    int count=0;
    for (int i = 0; i < winx.size(); i++) {
        for (int j = 0; j < winy.size(); j++) {
            for (int k = 0; k < winz.size(); k++) {
                
           
                VLOG(1)<<"["<<winx[i][0]<<", "<<winx[i][1]<<"], ["<<winy[j][0]<<", "<<winy[j][1]<<"], ["<<winz[k][0]<<", "<<winz[k][1]<<"]";
                frames[0].getWindow(winx[i][0], winx[i][1], winy[j][0], winy[j][1], winz[k][0], winz[k][1], i1, zero_padding_);
                frames[1].getWindow(winx[i][0], winx[i][1], winy[j][0], winy[j][1], winz[k][0], winz[k][1], i2, zero_padding_);
                convolve3D(i1, i2, i3, wx_, wy_, wz_);

                vector<int> mloc; double val;
                mloc = get_velocity_vector(i3, wx_, wy_, wz_, val);
                VLOG(1)<<mloc[0]<<", "<<mloc[1]<<", "<<mloc[2];
                count++;

                file<<winx[i][0]<<"\t"<<winy[j][0]<<"\t"<<winz[k][0]<<"\t"<<mloc[0]<<"\t"<<mloc[1]<<"\t"<<mloc[2]<<"\n";

            }
        }
    }

    double time = omp_get_wtime()-s;

    // print3D(i1, l, l, l);
    // print3D(i2, l, l, l);
    // print3D(i3, l, l, l);

    LOG(INFO)<<time<<", "<<time/count;

}

vector<int> piv3D::get_velocity_vector(double *a, int x, int y, int z, double &maxval) {

    maxval=0;
    int mx, my, mz;
    for (int i = 0; i < x; i++) {
        for (int j = 0; j < y; j++) {
            for (int k = 0; k < z; k++) {
                int ind = k+y*(j+z*i);
                if (a[ind]>maxval) {
                    maxval = a[ind];
                    mx = i; my = j; mz = k;
                }
            }
        }
    }

    vector<int> vector;
    vector.push_back(mx); vector.push_back(my); vector.push_back(mz);
    return(vector);

}

void piv3D::convolve3D(fftw_complex *a, fftw_complex *b, fftw_complex* &out, int x, int y, int z) {

    int n = x * y * z;
    fftw_complex *o1, *o2, *o3;
    o1 = new fftw_complex[n]; o2 = new fftw_complex[n]; o3 = new fftw_complex[n];

    fftw_plan r2c, c2r;
    r2c = fftw_plan_dft_3d(x, y, z, a, o1, -1, FFTW_ESTIMATE);
    c2r = fftw_plan_dft_3d(x, y, z, o3, out, 1, FFTW_ESTIMATE);

    // FFT steps
    fftw_execute_dft(r2c, a, o1);
    fftw_execute_dft(r2c, b, o2);

    // Multiply the FFTs
    multiply(o1, o2, o3, n);

    // Inverse FFT
    fftw_execute_dft(c2r, o3, out);
    normalize(out, n);

    fftw_destroy_plan(r2c); fftw_destroy_plan(c2r);

}

void piv3D::convolve3D(double *a, double *b, double* &out, int x, int y, int z) {

    int n = x * y * (z/2 +1);
    fftw_complex *o1, *o2, *o3;
    o1 = new fftw_complex[n]; o2 = new fftw_complex[n]; o3 = new fftw_complex[n];

    fftw_plan r2c, c2r;
    r2c = fftw_plan_dft_r2c_3d(x, y, z, a, o1, FFTW_ESTIMATE);
    c2r = fftw_plan_dft_c2r_3d(x, y, z, o3, out, FFTW_ESTIMATE);

    if (mean_shift_) {
        mean_shift(a, x*y*z);
        mean_shift(b, x*y*z);
    }

    // FFT steps
    fftw_execute_dft_r2c(r2c, a, o1);
    fftw_execute_dft_r2c(r2c, b, o2);

    // Multiply the FFTs
    multiply(o1, o2, o3, n);

    // Inverse FFT
    fftw_execute_dft_c2r(c2r, o3, out);
    normalize(out, x * y * z);

    fftw_destroy_plan(r2c); fftw_destroy_plan(c2r);
    delete [] o1; delete [] o2; delete [] o3;

}

void piv3D::convolve3D(double *a, double *b, double* &out, int x, int y, int z, fftw_plan r2c, fftw_plan c2r) {

    int n = x * y * (z/2 +1);
    fftw_complex *o1, *o2, *o3;
    o1 = new fftw_complex[n]; o2 = new fftw_complex[n]; o3 = new fftw_complex[n];

    if (mean_shift_) {
        mean_shift(a, x*y*z);
        mean_shift(b, x*y*z);
    }

    // FFT steps
    fftw_execute_dft_r2c(r2c, a, o1);
    fftw_execute_dft_r2c(r2c, b, o2);

    // Multiply the FFTs
    multiply(o1, o2, o3, n);

    // Inverse FFT
    fftw_execute_dft_c2r(c2r, o3, out);
    normalize(out, x * y * z);

    delete [] o1;
    delete [] o2;
    delete [] o3;

}

void piv3D::multiply(fftw_complex *a, fftw_complex *b, fftw_complex*& out, int n) {

    for (int i = 0; i < n; i++) {
        out[i][0] = a[i][0]*b[i][0] - a[i][1]*b[i][1];
        out[i][1] = a[i][0]*b[i][1] + a[i][1]*b[i][0];
    }

}

void piv3D::normalize(fftw_complex*& a, int n) {

    for (int i = 0; i < n; i++) {
        a[i][0] /= n; a[i][1] /= n;
    }

}

void piv3D::normalize(double*& a, int n) {

    for (int i = 0; i < n; i++)
        a[i] /= n;

}

void piv3D::print3D(double *a, int x, int y, int z) {

    for (int k = 0; k < z; k++) {
        cout<<"[";
        for (int i = 0; i < x; i++) {
            for (int j = 0; j < y; j++) {
                cout<<a[k+y*(j+x*i)];
                if (j<y-1)
                    cout<<",\t";
                else
                    cout<<";";
            }
            if (i<x-1)
                cout<<"\n";
            else
                cout<<"]\n";
        }
    }

}

void piv3D::print3D(fftw_complex *a, int x, int y, int z) {

    for (int k = 0; k < z; k++) {
        
        for (int i = 0; i < x; i++) {
            for (int j = 0; j < y; j++) {
                cout<<a[k+y*(j+x*i)][0]<<" + "<<a[k+y*(j+x*i)][1]<<"i,\t";
            }
            cout<<endl;
        }
        
    }

}

void piv3D::mean_shift(double*& a, int n) {

    double sum = 0;

    for (int i = 0; i < n; i++)
        sum += a[i];
    
    sum /= n;

    for (int i = 0; i < n; i++)
        a[i] -= sum;
    

}

vector< vector<int> > piv3D::get_windows(int s, int w, double overlap) {

    vector< vector<int> > outer;
    vector<int> inner;

    int start = 0; int end = start+w-1;
    inner.push_back(start); inner.push_back(end);
    outer.push_back(inner); inner.clear();

    while (end<s-1) {
        start += w*(1-overlap);
        end += w*(1-overlap);
        inner.push_back(start); inner.push_back(end);
        outer.push_back(inner); inner.clear();
    }

    return(outer);

}

Mat3::Mat3(vector<Mat> volume): volume_(volume) {

}

void Mat3::getWindow(int x1, int x2, int y1, int y2, int z1, int z2, double*& win, int zero_padding) {

    int nx = x2 - x1 + 1; int ny = y2 - y1 + 1; int nz = z2 - z1 + 1;
    int sx = 0; int sy = 0; int sz = 0;

    if (zero_padding) {
        sx = nx/2; sy = ny/2; sz = nz/2;
        nx *= 2; ny *= 2; nz *= 2;

        for (int i = 0; i <= nx; i++) {
            for (int j = 0; j <= ny; j++) {
                for (int k = 0; k <= nz; k++) {
                    int ind = (k + ny*(j + nx*i));
                    win[ind] = 0;
                }
            }
        }

    }

    for (int i = x1; i <= x2; i++) {
        for (int j = y1; j <= y2; j++) {
            for (int k = z1; k <= z2; k++) {
                int ind = (k+sz-z1) + ny*((j+sy-y1) + nx*(i+sx-x1));
                win[ind] = volume_[k].at<double>(i,j);
            }
        }
    }

}
