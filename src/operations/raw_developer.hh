/* 
 */

/*

    Copyright (C) 2014 Ferrero Andrea

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.


 */

/*

    These files are distributed with PhotoFlow - http://aferrero2707.github.io/PhotoFlow/

 */

#ifndef VIPS_RAW_DEVELOPER_H
#define VIPS_RAW_DEVELOPER_H

#include <string>

#include <libraw/libraw.h>

#include "../base/processor.hh"
#include "../base/imagepyramid.hh"
#include "../base/rawmatrix.hh"

#include "raw_image.hh"

namespace PF 
{

	enum demo_method_t {
		PF_DEMO_FAST,
		PF_DEMO_AMAZE,
		PF_DEMO_IGV
	};

  class RawDeveloperPar: public OpParBase
  {
    VipsBandFormat output_format;
    dcraw_data_t* image_data;
    PF::ProcessorBase* amaze_demosaic;
    PF::ProcessorBase* igv_demosaic;
    PF::ProcessorBase* fast_demosaic;
    PF::ProcessorBase* raw_preprocessor;
    PF::ProcessorBase* raw_output;
    PF::ProcessorBase* convert_format;
    PF::ProcessorBase* fcs[4];

		// False color suppression steps
		PropertyBase demo_method;
		// False color suppression steps
		Property<int> fcs_steps;

  public:
    RawDeveloperPar();

    /* Set processing hints:
       1. the intensity parameter makes no sense for an image, 
          creation of an intensity map is not allowed
       2. the operation can work without an input image;
          the blending will be set in this case to "passthrough" and the image
	  data will be simply linked to the output
     */
    bool has_intensity() { return false; }
    bool has_opacity() { return false; }
    bool needs_input() { return true; }
    //bool needs_caching() { return true; }

    dcraw_data_t* get_image_data() {return image_data; }

    VipsImage* build(std::vector<VipsImage*>& in, int first, 
		     VipsImage* imap, VipsImage* omap, unsigned int& level);
  };

  

  template < OP_TEMPLATE_DEF > 
  class RawDeveloper
  {
  public: 
    void render(VipsRegion** ireg, int n, int in_first,
		VipsRegion* imap, VipsRegion* omap, 
		VipsRegion* oreg, OpParBase* par)
    {
			/*
      RawDeveloperPar* rdpar = dynamic_cast<RawDeveloperPar*>(par);
      if( !rdpar ) return;
      dcraw_data_t* image_data = rdpar->get_image_data();
      Rect *r = &oreg->valid;
      //int sz = oreg->im->Bands;//IM_REGION_N_ELEMENTS( oreg );
      //int line_size = r->width * oreg->im->Bands; //layer->in_all[0]->Bands; 
    
      PF::raw_pixel_t* p;
      PF::raw_pixel_t* pout;
      int x, y;
      float range = image_data->color.maximum - image_data->color.black;
      float min_mul = image_data->color.cam_mul[0];
      float max_mul = image_data->color.cam_mul[0];
      for(int i = 1; i < 4; i++) {
	if( image_data->color.cam_mul[i] < min_mul )
	  min_mul = image_data->color.cam_mul[i];
	if( image_data->color.cam_mul[i] > max_mul )
	  max_mul = image_data->color.cam_mul[i];
      }
#ifndef NDEBUG
      if( r->top==0 && r->left==0 ) {
	std::cout<<"RawDeveloper::render(): input format="<<ireg[in_first]->im->BandFmt
		 <<"  output format="<<oreg->im->BandFmt
		 <<std::endl;
      }
#endif
      std::cout<<"range="<<range<<"  min_mul="<<min_mul<<"  new range="<<range*min_mul<<std::endl;
      // RawTherapee emulation
      //range *= max_mul;
      
      range *= min_mul;
    
      for( y = 0; y < r->height; y++ ) {
	p = (PF::raw_pixel_t*)VIPS_REGION_ADDR( ireg[in_first], r->left, r->top + y ); 
	pout = (PF::raw_pixel_t*)VIPS_REGION_ADDR( oreg, r->left, r->top + y ); 
		PF::RawMatrixRow rp( p );
		PF::RawMatrixRow rpout( pout );
	for( x=0; x < r->width; x++) {
	  //std::cout<<"x: "<<x<<"  y: "<<y<<"  color: "<<(int)p[x].color<<std::endl;
	  rpout.color(x) = rp.color(x);
	  rpout[x] = rp[x] * image_data->color.cam_mul[ rp.color(x) ];
	  rpout[x] /= range;
			*/
	  /* RawTherapee emulation
	  pout[x].data *= 65535;
	  */
	  /*
	  std::cout<<"data: input="<<p[x].data<<"  output="<<pout[x].data
		   <<"  cam_mul="<<image_data->color.cam_mul[ p[x].color ]
		   <<"  range="<<range<<std::endl;
	  */
			/*
		}
	}
			*/
		}
	};




  ProcessorBase* new_raw_developer();
}

#endif 


