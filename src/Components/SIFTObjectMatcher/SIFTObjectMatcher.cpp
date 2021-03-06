/*!
 * \file
 * \brief
 * \author Micha Laszkowski
 */

#include <memory>
#include <string>
#include <iomanip>

#include "SIFTObjectMatcher.hpp"
#include "Common/Logger.hpp"
#include <pcl/registration/correspondence_estimation.h>
#include <pcl/registration/correspondence_rejection_sample_consensus.h>
#include <pcl/impl/instantiate.hpp>
#include <pcl/search/kdtree.h> 
#include <pcl/search/impl/kdtree.hpp>
#include <boost/bind.hpp>

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/features2d/features2d.hpp>
#include <opencv2/highgui/highgui.hpp>
#include "Types/Features.hpp"

namespace Processors {
namespace SIFTObjectMatcher {

class SIFTFeatureRepresentation: public pcl::DefaultFeatureRepresentation<PointXYZSIFT> //could possibly be pcl::PointRepresentation<...> ??
{
    using pcl::PointRepresentation<PointXYZSIFT>::nr_dimensions_;
    public:
    SIFTFeatureRepresentation ()
    {
        // Define the number of dimensions
        nr_dimensions_ = 128 ;
        trivial_ = false ;
    }

    // Override the copyToFloatArray method to define our feature vector
    virtual void copyToFloatArray (const PointXYZSIFT &p, float * out) const
    {
        //This representation is only for determining correspondences (not for use in Kd-tree for example - so use only SIFT part of the point
        for (register int i = 0; i < 128 ; i++)
            out[i] = p.descriptor[i];//p.descriptor.at<float>(0, i) ;
        //std::cout << "SIFTFeatureRepresentation:copyToFloatArray()" << std::endl ;
    }
};

SIFTObjectMatcher::SIFTObjectMatcher(const std::string & name) :
		Base::Component(name),
		threshold("threshold", 0.75f)  {
			registerProperty(threshold);
}

SIFTObjectMatcher::~SIFTObjectMatcher() {
}

void SIFTObjectMatcher::prepareInterface() {
	// Register data streams, events and event handlers HERE!
	registerStream("in_models", &in_models);
	registerStream("in_cloud_xyzsift", &in_cloud_xyzsift);
	registerStream("in_cloud_xyzrgb", &in_cloud_xyzrgb);
	// Register handlers
	h_readModels.setup(boost::bind(&SIFTObjectMatcher::readModels, this));
	registerHandler("readModels", &h_readModels);
	addDependency("readModels", &in_models);
	h_match.setup(boost::bind(&SIFTObjectMatcher::match, this));
	registerHandler("match", &h_match);
	addDependency("match", &in_cloud_xyzsift);
	addDependency("match", &in_cloud_xyzrgb);

}

bool SIFTObjectMatcher::onInit() {

	return true;
}

bool SIFTObjectMatcher::onFinish() {
	return true;
}

bool SIFTObjectMatcher::onStop() {
	return true;
}

bool SIFTObjectMatcher::onStart() {
	return true;
}

void SIFTObjectMatcher::readModels() {
	cout<<"readModels()"<<endl;
	for( int i = 0 ; i<models.size(); i++){
		delete models[i];
	}
	models.clear();
	std::vector<AbstractObject*> abstractObjects = in_models.read();
	for( int i = 0 ; i<abstractObjects.size(); i++){
		cout<<"Name: "<<abstractObjects[i]->name<<endl;
		SIFTObjectModel *model = dynamic_cast<SIFTObjectModel*>(abstractObjects[i]);
		if(model!=NULL)
			models.push_back(model);
		else
			cout<<"niepoprawny model"<<endl;
	}
	cout<<models.size()<<" modeli"<<endl;
}

void SIFTObjectMatcher::match() {
	CLOG(LTRACE) << "SIFTObjectMatcher::match()"<<endl;
	if(models.empty()){
		cout<<"No models available" <<endl;
		return;
	}
			
	pcl::PointCloud<PointXYZSIFT>::Ptr cloud_xyzsift = in_cloud_xyzsift.read();
	pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud_xyzrgb = in_cloud_xyzrgb.read();	
	

		for (int i = 0 ; i<models.size(); i++){
			CLOG(LTRACE) << "liczba cech modelu "<<i<<" "<<models[i]->name<<": " <<
				models[i]->cloud_xyzsift->size()<<endl; 	
		}
		CLOG(LTRACE) << "liczba cech instancji : " <<
			cloud_xyzsift->size()<<endl; 
    //////////////flann
    //cv::Mat descriptors_instance(cloud_xyzsift->size(), 128, CV_32F,0.0);
    //cout<<cloud_xyzsift->size()<<endl;
    //for (int i=0; i< cloud_xyzsift->size(); i++)
        //for(int j=0; j<128;j++){
            //descriptors_instance.at<float>(i,j)=cloud_xyzsift->at(i).descriptor[j];
        //}
	/////////////

        pcl::CorrespondencesPtr correspondences(new pcl::Correspondences()) ;
        pcl::registration::CorrespondenceEstimation<PointXYZSIFT, PointXYZSIFT> correst ;

        //SIFTFeatureRepresentation point_representation ;
        //correst.setPointRepresentation (point_representation.makeShared()); //NEVER do like this, makeShared will return DefaultFeatureRepresentation<PointDefault>!
        SIFTFeatureRepresentation::Ptr point_representation(new SIFTFeatureRepresentation()) ;
        correst.setPointRepresentation(point_representation) ;
        for (int i = 0 ; i<models.size(); i++){
            correst.setInputSource(cloud_xyzsift) ;
            correst.setInputTarget(models[i]->cloud_xyzsift) ;
            correst.determineReciprocalCorrespondences(*correspondences) ;
			//ransac - niepoprawne dopasowania
			pcl::Correspondences inliers ;
        	pcl::registration::CorrespondenceRejectorSampleConsensus<PointXYZSIFT> sac ;
			sac.setInputSource(cloud_xyzsift) ;
			sac.setInputTarget(models[i]->cloud_xyzsift) ;
			sac.setInlierThreshold(0.001f) ;
			sac.setMaximumIterations(2000) ;
			sac.setInputCorrespondences(correspondences) ;
			sac.getCorrespondences(inliers) ;
			//std::cout << "SAC inliers " << inliers.size() << std::endl ;

            CLOG(LTRACE) << setprecision(2) << fixed;
            float percent = ((float)correspondences->size() - (float)inliers.size())/(float)models[i]->mean_viewpoint_features_number;
            CLOG(LTRACE)  << "\nNumber of reciprocal correspondences: " << correspondences->size() <<". Bad correspondences:" << inliers.size() ;
            CLOG(LTRACE)  << " out of " << cloud_xyzsift->size() << " keypoints of instance, = " ;
            CLOG(LTRACE)  << percent << ". "<< models[i]->cloud_xyzsift->size() << " keypoints of model "<< models[i]->name << std::endl ;
            if (percent > threshold)
				std::cout <<"Rozpoznano model "<< models[i]->name<<endl;
        /////////////////////////////

        ///////////////////////////////////
            //flann
            /////////////////////////////////////
            //cv::Mat descriptors_model(models[i]->SIFTcloud->size(), 128, CV_32F,0.0);
            //for (int j=0; j< models[i]->SIFTcloud->size(); j++)
                //for(int k=0; k<128;k++){
                    //descriptors_model.at<float>(j,k)=models[i]->SIFTcloud->at(j).descriptor[k];
                //}

            //cv::FlannBasedMatcher matcher;
            //std::vector< cv::DMatch > matches;

            //matcher.match( descriptors_instance, descriptors_model, matches );
            ///////////////////////////////////
            //double max_dist = 0; double min_dist = 100;

             ////-- Quick calculation of max and min distances between keypoints
             //for( int j = 0; j < descriptors_instance.rows; j++ )
             //{ double dist = matches[j].distance;
               //if( dist < min_dist ) min_dist = dist;
               //if( dist > max_dist ) max_dist = dist;
             //}
             //std::vector< cv::DMatch > good_matches;

             //for( int j = 0; j < descriptors_instance.rows; j++ ){
                 //if( matches[j].distance <= max(2*min_dist, 0.02) )
                 //{ good_matches.push_back( matches[j]); }
             //}
             //cout<<"Matches: "<< matches.size()<< ". Good matches: "<<good_matches.size()<<endl;
             //procent = (float)good_matches.size()/(float)cloud_xyzsift->size();
             //cout<<"Procent: "<<procent<<endl;
        }
}



} //: namespace SIFTObjectMatcher
} //: namespace Processors
