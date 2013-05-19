// -------------------------------------------------------
// -------------------------------------------------------
// Synthetic Aperture - Particle Tracking Velocimetry Code
// --- Particle Tracking ---
// -------------------------------------------------------
// Author: Abhishek Bajpayee
//         Dept. of Mechanical Engineering
//         Massachusetts Institute of Technology
// -------------------------------------------------------
// -------------------------------------------------------

#include "std_include.h"
#include "refocusing.h"
#include "tracking.h"
#include "typedefs.h"
#include "visualize.h"
#include "tools.h"

using namespace std;
using namespace cv;

void pTracking::read_points(string path) {
    
    Point3f point;
    volume vol;

    ifstream file;
    file.open(path.c_str());

    cout<<"\nReading points to track...\n";
    
    int num_frames = 30;

    for (int i=0; i<num_frames; i++) {

        int num_points;
        file>>num_points;

        vol.x1 = 0;
        vol.x2 = 0;
        vol.y1 = 0;
        vol.y2 = 0;
        vol.z1 = 0;
        vol.z2 = 0;

        for (int j=0; j<num_points; j++) {
            
            file>>point.x;
            file>>point.y;
            file>>point.z;

            if (point.x>vol.x2) vol.x2 = point.x;
            if (point.y>vol.y2) vol.y2 = point.y;
            if (point.z>vol.z2) vol.z2 = point.z;
            if (point.x<vol.x1) vol.x1 = point.x;
            if (point.y<vol.y1) vol.y1 = point.y;
            if (point.z<vol.z1) vol.z1 = point.z;

            points_.push_back(point);
        }

        all_points_.push_back(points_);
        vols_.push_back(vol);
        points_.clear();

    }

    cout<<"done!\n";

}

void pTracking::track() {

    PyVisualize vis;

    int f1, f2;
    f1 = 0;
    f2 = 1;

    double A, B, C, D, E, F;
    A = 0.3;
    B = 3.0;
    C = 0.1;
    D = 5.0;
    E = 1.0;
    F = 0.05;

    double R_n, R_s;
    R_n = 20.0;
    R_s = 20.0;

    int n1, n2;
    n1 = all_points_[f1].size();
    n2 = all_points_[f2].size();

    cout<<"Neighbor sets...\n";
    vector< vector<int> > S_r = neighbor_set(f1, f1, R_n);
    vector< vector<int> > S_c = neighbor_set(f1, f2, R_s);

    vector<Mat> Pij, Pi, Pij2, Pi2;
    cout<<"Probability sets...\n";
    build_probability_sets(S_r, S_c, Pij, Pi, Pij2, Pi2);

    cout<<"Relaxation sets...\n";
    vector< vector< vector< vector<Point2i> > > > theta;
    build_relaxation_sets(f1, f2, S_r, S_c, C, D, E, F, theta);
    
    cout<<"Solving...\n";

    int N = 20;

    for (int n=0; n<N; n++) {

        for (int k=0; k<Pij.size(); k++) {

            for (int i=0; i<Pij[k].rows; i++) {
                for (int j=0; j<Pij[k].cols; j++) {

                    double sum = 0;
                    for (int l=0; l<theta[k][i][j].size(); l++) {
                        sum += Pij[k].at<double>(theta[k][i][j][l].x, theta[k][i][j][l].y);
                    }

                    double newval = (Pij[k].at<double>(i,j))*(A + (B*sum));
                    Pij2[k].at<double>(i,j) = newval;

                    int draw=0;
                    if (draw) {
                        vis.figure();
                        vis.scatter3d(all_points_[f1], S_r[k], "20", "r");
                        vis.hold(1);
                        vis.scatter3d(all_points_[f2], S_c[k], "20", "b");
                        vis.line3d(all_points_[f1][S_r[k][i]], all_points_[f2][S_c[k][j]]);
                        for (int l=0; l<theta[k][i][j].size(); l++) {
                            vis.line3d(all_points_[f1][S_r[k][theta[k][i][j][l].x]], all_points_[f2][S_c[k][theta[k][i][j][l].y]]);
                        }
                        vis.show();
                        vis.clear();
                    }

                }
            }

        }

        normalize_probabilites(Pij2,Pi2);
        
        /*
        cout<<"\nprobs\n";
        for (int i=0; i<Pij2[k].rows; i++) {
            double sum = 0;
            for (int j=0; j<Pij2[k].cols; j++) {
                printf("%f ", Pij2[k].at<double>(i,j));
                sum += Pij2[k].at<double>(i,j);
            }
            printf("%f --- ", Pi2[k].at<double>(i,0));
            sum += Pi2[k].at<double>(i,0);
            printf("%f\n", sum);
        }
        cout<<"diffsum: "<<diffsum<<"\n\n";
        */

        update_probabilities(Pij, Pi, Pij2, Pi2);

    }

    vector<Point2i> matches;
    for (int i=0; i<Pij.size(); i++) {
        for (int j=0; j<S_r[i].size(); j++) {

            if (i == S_r[i][j]) {
                int found = 0;
                for (int k=0; k<S_c[i].size(); k++) {
                    if (Pij[i].at<double>(j,k)>0.99) {
                        matches.push_back(Point2i(i,S_c[i][k]));
                        found = 1;
                        break;
                    }   
                }
                if (!found) matches.push_back(Point2i(i,-1));
                
            }

        }
    }

    vis.figure();
    for (int i=0; i<matches.size(); i++) {
        if (matches[i].y>=0) vis.line3d(all_points_[f1][matches[i].x], all_points_[f2][matches[i].y]);
    }
    vis.show();

}

void pTracking::update_probabilities(vector<Mat> &Pij, vector<Mat> &Pi, vector<Mat> &Pij2, vector<Mat> &Pi2) {

    for (int i=0; i<Pij.size(); i++) {
        Pij[i] = Pij2[i].clone();
        Pi[i] = Pi2[i].clone();
    }

}

void pTracking::normalize_probabilites(vector<Mat> &Pij, vector<Mat> &Pi) {

    for (int i=0; i<Pij.size(); i++) {

        for (int j=0; j<Pij[i].rows; j++) {
            double sum = 0;
            for (int k=0; k<Pij[i].cols; k++) {
                sum += Pij[i].at<double>(j,k);
            }
            sum += Pi[i].at<double>(j,0);
            for (int k=0; k<Pij[i].cols; k++) {
                Pij[i].at<double>(j,k) /= sum;
            }
            Pi[i].at<double>(j,0) /= sum;
        }

    }

}

void pTracking::build_probability_sets(vector< vector<int> > S_r, vector< vector<int> > S_c, vector<Mat> &Pij, vector<Mat> &Pi, vector<Mat> &Pij2, vector<Mat> &Pi2) {

    for (int i=0; i<S_r.size(); i++) {
        
        Mat_<double> Pij_single = Mat_<double>::zeros(S_r[i].size(), S_c[i].size());
        Mat_<double> Pi_single = Mat_<double>::zeros(S_r[i].size(), 1);

        for (int j=0; j<Pij_single.rows; j++) {
            for (int k=0; k<Pij_single.cols; k++) {
                Pij_single(j,k) = 1.0/(double(Pij_single.cols)+1.0);
            }
            Pi_single(j,0) = 1.0/(double(Pij_single.cols)+1.0);
        }

        Pij.push_back(Pij_single.clone());
        Pij2.push_back(Pij_single.clone());
        Pi.push_back(Pi_single.clone());
        Pi2.push_back(Pi_single.clone());

    }

}

void pTracking::build_relaxation_sets(int frame1, int frame2, vector< vector<int> > S_r, vector< vector<int> > S_c, double C, double D, double E, double F, vector< vector< vector< vector<Point2i> > > > &theta) {

    vector<Point2i> theta_single;
    vector< vector<Point2i> > theta_j;
    vector< vector< vector<Point2i> > > theta_ij;
    Point3f dij, dkl;
    double dij_mag;

    for (int n=0; n<S_r.size(); n++) {
        for (int i=0; i<S_r[n].size(); i++) {
            for (int j=0; j<S_c[n].size(); j++) {
                for (int k=0; k<S_r[n].size(); k++) {
                    for (int l=0; l<S_c[n].size(); l++) {

                        dij = Point3f(all_points_[frame1][S_r[n][i]].x-all_points_[frame2][S_c[n][j]].x, all_points_[frame1][S_r[n][i]].y-all_points_[frame2][S_c[n][j]].y, all_points_[frame1][S_r[n][i]].z-all_points_[frame2][S_c[n][j]].z);

                        dkl = Point3f(all_points_[frame1][S_r[n][k]].x-all_points_[frame2][S_c[n][l]].x, all_points_[frame1][S_r[n][k]].y-all_points_[frame2][S_c[n][l]].y, all_points_[frame1][S_r[n][k]].z-all_points_[frame2][S_c[n][l]].z);

                        dij_mag = dist(all_points_[frame1][S_r[n][i]], all_points_[frame2][S_c[n][j]]);

                        if ( dist(dij, dkl) < (E+(F*dij_mag)) ) theta_single.push_back(Point2i(k,l));

                    }
                }
                theta_j.push_back(theta_single);
                theta_single.clear();
            }
            theta_ij.push_back(theta_j);
            theta_j.clear();
        }
        theta.push_back(theta_ij);
        theta_ij.clear();
    }

}

vector< vector<int> > pTracking::neighbor_set(int frame1, int frame2, double r) {

    vector<int> indices;
    vector< vector<int> > indices_all;

    for (int i=0; i<all_points_[frame1].size(); i++) {
        for (int j=0; j<all_points_[frame2].size(); j++) {
            if (dist(all_points_[frame1][i], all_points_[frame2][j]) < r) indices.push_back(j);
        }
        indices_all.push_back(indices);
        indices.clear();
    }

    return(indices_all);

}

vector<int> pTracking::points_in_region(int frame, Point3f center, double r) {

    vector<int> indices;

    for (int i=0; i<all_points_[frame].size(); i++) {
        if (dist(all_points_[frame][i], center)<=r) indices.push_back(i);
    }

    return(indices);

}
