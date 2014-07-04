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

#include <fstream>
#include <algorithm>

#include <gtk/gtk.h>

#include "image.hh"
#include "imageprocessor.hh"
#include "pf_file_loader.hh"
#include "../operations/convert2srgb.hh"
#include "../operations/convertformat.hh"


static bool getFileExtension(const char * dir_separator, const std::string & file, std::string & ext)
{
    std::size_t ext_pos = file.rfind(".");
    std::size_t dir_pos = file.rfind(dir_separator);

    if(ext_pos>dir_pos+1)
    {
        ext.append(file.begin()+ext_pos+1,file.end());
	std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        return true;
    }

    return false;
}



gint PF::image_rebuild_callback( gpointer data )
{
  PF::Image* image = (PF::Image*)data;
#ifndef NDEBUG
  //std::cout<<"PF::image_rebuild_callback(): called, image->is_modified()="<<image->is_modified()<<std::endl;
#endif
  if( image->is_modified() ) {

    bool result = image->get_layer_manager().rebuild_prepare();
    /*
#ifndef NDEBUG
    std::cout<<"PF::image_rebuild_callback(): rebuild prepare "<<(result?"OK":"failed")<<std::endl;
#endif
    if( !result ) {
      // something wrong here, we release all the locks and stop the update
      // in fact, this should never happen...
      for( unsigned int i = 0; i < image->get_nviews(); i++ ) {
	PF::View* view = image->get_view( i );
	if( !view ) continue;
	view->unlock_processing();
      }
      return false;
    }
    */
    
    image->set_modified( false );

    // Loop on views, re-build and update
    for( unsigned int i = 0; i < image->get_nviews(); i++ ) {
      PF::View* view = image->get_view( i );
      if( !view ) continue;

#ifndef NDEBUG
      std::cout<<"PF::image_rebuild_callback(): updating view #"<<i<<std::endl;
#endif
      //vips_cache_drop_all();
      image->get_layer_manager().rebuild( view, PF::PF_COLORSPACE_RGB, 100, 100 );
      view->update();
    }

    image->get_layer_manager().rebuild_finalize();
  }
  return false;
}


PF::Image::Image(): 
  layer_manager( this ), 
  async( false ), 
  modified( false ), 
  rebuilding( false ) 
{
  rebuild_mutex = vips_g_mutex_new();
  g_mutex_lock( rebuild_mutex );
  rebuild_done = vips_g_cond_new();
  layer_manager.signal_modified.connect(sigc::mem_fun(this, &Image::update) );
  convert2srgb = new PF::Processor<PF::Convert2sRGBPar,PF::Convert2sRGBProc>();
  convert_format = new PF::Processor<PF::ConvertFormatPar,PF::ConvertFormatProc>();
}

PF::Image::~Image()
{
  for( unsigned int vi = 0; vi < views.size(); vi++ ) {
    if( views[vi] != NULL )
      delete views[vi];
  }
}


void PF::Image::update()
{
  if( PF::PhotoFlow::Instance().is_batch() ) {
    do_update();
  } else {
    ProcessRequestInfo request;
    request.image = this;
    request.request = PF::IMAGE_REBUILD;
    
#ifndef NDEBUG
    std::cout<<"PF::Image::update(): submitting rebuild request."<<std::endl;
#endif
    PF::ImageProcessor::Instance().submit_request( request );

    g_cond_wait( rebuild_done, rebuild_mutex );
  }

  /*
  if( is_async() )
    update_async();
  else
    update_sync();
  */
}


void PF::Image::do_update()
{
  //std::cout<<"PF::Image::do_update(): is_modified()="<<is_modified()<<std::endl;
  //if( !is_modified() ) return;

  std::cout<<std::endl<<"============================================"<<std::endl;
  //std::cout<<"PF::Image::do_update(): is_modified()="<<is_modified()<<std::endl;
  bool result = get_layer_manager().rebuild_prepare();
  /*
    #ifndef NDEBUG
    std::cout<<"PF::image_rebuild_callback(): rebuild prepare "<<(result?"OK":"failed")<<std::endl;
    #endif
    if( !result ) {
    // something wrong here, we stop the update
    // in fact, this should never happen...
    return false;
    }
  */
    
  set_modified( false );

  // Loop on views, re-build and update
  for( unsigned int i = 0; i < get_nviews(); i++ ) {
    PF::View* view = get_view( i );
    if( !view ) continue;

    //#ifndef NDEBUG
    std::cout<<"PF::Image::do_update(): updating view #"<<i<<std::endl;
    //#endif
    get_layer_manager().rebuild( view, PF::PF_COLORSPACE_RGB, 100, 100 );
    //view->update();
  }

  get_layer_manager().rebuild_finalize();

  for( unsigned int i = 0; i < get_nviews(); i++ ) {
    PF::View* view = get_view( i );
    if( !view ) continue;
    std::cout<<"PF::Image::do_update(): ref_counts of view #"<<i<<std::endl;
    for(int ni = 0; ni < view->get_nodes().size(); ni++ ) {
      PF::ViewNode* node = view->get_nodes()[ni];
      if( !node ) continue;
      std::cout<<"  node #"<<ni<<" ("<<(void*)node->image<<") = "<<G_OBJECT(node->image)->ref_count<<std::endl;
    }
  }
  std::cout<<std::endl<<"============================================"<<std::endl<<std::endl<<std::endl;
}



void PF::Image::remove_layer( PF::Layer* layer )
{
  remove_from_inputs( layer );
  std::list<Layer*>* list = layer_manager.get_list( layer );
  if( list )
    remove_layer( layer, *list );
}


void PF::Image::remove_from_inputs( PF::Layer* layer )
{
  remove_from_inputs( layer, layer_manager.get_layers() );
}


void PF::Image::remove_from_inputs( PF::Layer* layer, std::list<Layer*>& list )
{
  std::list<PF::Layer*>::iterator li;
  for(li = list.begin(); li != list.end(); ++li) {
    PF::Layer* l = *li;
    l->remove_input( layer->get_id() );
    remove_from_inputs( layer, l->get_sublayers() );
    remove_from_inputs( layer, l->get_imap_layers() );
    remove_from_inputs( layer, l->get_omap_layers() );
  }  
}


void PF::Image::remove_layer( PF::Layer* layer, std::list<Layer*>& list )
{
  std::vector<View*>::iterator vi;
  for( vi = views.begin(); vi != views.end(); vi++ ) {
    (*vi)->remove_node( layer->get_id() );
  }  

  std::list<Layer*>::iterator it;
  std::list<Layer*> sublayers = layer->get_sublayers();
  for( it = sublayers.begin(); it != sublayers.end(); it++ ) {
    remove_layer( *it, layer->get_sublayers() );
  }

  sublayers = layer->get_imap_layers();
  for( it = sublayers.begin(); it != sublayers.end(); it++ ) {
    remove_layer( *it, layer->get_imap_layers() );
  }

  sublayers = layer->get_omap_layers();
  for( it = sublayers.begin(); it != sublayers.end(); it++ ) {
    remove_layer( *it, layer->get_omap_layers() );
  }

  std::list<PF::Layer*>::iterator li;
  for(li = list.begin(); li != list.end(); ++li) {
    PF::Layer* l = *li;
    if( l->get_id() == layer->get_id() ) {
      list.erase( li );
      break;
    }
  }

  layer_manager.delete_layer( layer );
}


bool PF::Image::open( std::string filename )
{
  std::string ext;
  if( !getFileExtension( "/", filename, ext ) ) return false;
  if( ext == "pfi" ) {

    PF::load_pf_image( filename, this );
    //PF::PhotoFlow::Instance().set_image( pf_image );
    //layersWidget.set_image( pf_image );
    //add_view( VIPS_FORMAT_UCHAR, 0 );
    add_view( VIPS_FORMAT_FLOAT, 0 );
    add_view( VIPS_FORMAT_USHORT, 0 );

  } else if( ext=="tiff" || ext=="tif" || ext=="jpg" || ext=="jpeg" ) {

    //PF::PhotoFlow::Instance().set_image( pf_image );
    //layersWidget.set_image( pf_image );

    add_view( VIPS_FORMAT_USHORT, 0 );
    add_view( VIPS_FORMAT_USHORT, 0 );

    PF::Layer* limg = layer_manager.new_layer();
    PF::ProcessorBase* proc = PF::PhotoFlow::Instance().new_operation( "imageread", limg );
    if( proc->get_par() && proc->get_par()->get_property( "file_name" ) )
      proc->get_par()->get_property( "file_name" )->set_str( filename );
    limg->set_processor( proc );
    limg->set_name( "background" );
    layer_manager.get_layers().push_back( limg );

    /*
    PF::Processor<PF::ImageReaderPar,PF::ImageReader>* imgread = 
      new PF::Processor<PF::ImageReaderPar,PF::ImageReader>();
    imgread->get_par()->set_file_name( filename );

    PF::Layer* limg = layer_manager.new_layer();
    limg->set_processor( imgread );
    limg->set_name( "background" );

    PF::ImageReadConfigDialog* img_config = 
      new PF::ImageReadConfigDialog( limg );
    imgread->get_par()->set_config_ui( img_config );
    //img_config->set_layer( limg );
    //img_config->set_image( pf_image );

    //layer_manager.get_layers().push_back( limg );
    layersWidget.add_layer( limg );
    */
  } else {
    
    add_view( VIPS_FORMAT_FLOAT, 0 );
    add_view( VIPS_FORMAT_USHORT, 0 );

    PF::Layer* limg = layer_manager.new_layer();
    PF::ProcessorBase* proc = PF::PhotoFlow::Instance().new_operation( "raw_loader", limg );
    if( proc->get_par() && proc->get_par()->get_property( "file_name" ) )
      proc->get_par()->get_property( "file_name" )->set_str( filename );
    limg->set_processor( proc );
    limg->set_name( "RAW loader" );
    layer_manager.get_layers().push_back( limg );

    limg = layer_manager.new_layer();
    proc = PF::PhotoFlow::Instance().new_operation( "raw_developer", limg );
    limg->set_processor( proc );
    limg->set_name( "RAW developer" );
    layer_manager.get_layers().push_back( limg );
    /*
    limg = layer_manager.new_layer();
    proc = PF::PhotoFlow::Instance().new_operation( "raw_output", limg );
    limg->set_processor( proc );
    limg->set_name( "RAW output" );
    layer_manager.get_layers().push_back( limg );
    */
  }

  //imageArea.set_view( pf_image->get_view(0) );
  //pf_image->signal_modified.connect( sigc::mem_fun(&imageArea, &ImageArea::update_image) );
  //sleep(5);
  update();
}


bool PF::Image::save( std::string filename )
{
  std::string ext;
  if( getFileExtension( "/", filename, ext ) &&
      ext == "pfi" ) {

    std::ofstream of;
    of.open( filename.c_str() );
    if( !of ) return false;
    of<<"<image>"<<std::endl;
    layer_manager.save( of );
    of<<"</image>"<<std::endl;
    return true;
  } else {
    return false;
  }
}



bool PF::Image::export_merged( std::string filename )
{
  std::string ext;
  if( getFileExtension( "/", filename, ext ) &&
      ext != "pfi" ) {
    //Glib::Threads::Mutex::Lock lock( rebuild_mutex );
    unsigned int level = 0;
    PF::View* view = new PF::View( this, VIPS_FORMAT_USHORT, 0 );
    layer_manager.rebuild_all( view, PF::PF_COLORSPACE_RGB, 100, 100 );
    VipsImage* image = view->get_output();
    VipsImage* outimg = image;

    std::vector<VipsImage*> in;
    /**/
    convert2srgb->get_par()->set_image_hints( image );
    convert2srgb->get_par()->set_format( view->get_format() );
    in.clear(); in.push_back( image );
    VipsImage* srgbimg = convert2srgb->get_par()->build(in, 0, NULL, NULL, level );
    //g_object_unref( image );
    std::string msg = std::string("PF::Image::export_merged(") + filename + "), image";
    PF_UNREF( image, msg.c_str() );
    /**/
    //VipsImage* srgbimg = image;

    in.clear();
    in.push_back( srgbimg );
    convert_format->get_par()->set_image_hints( srgbimg );
    convert_format->get_par()->set_format( VIPS_FORMAT_UCHAR );
    outimg = convert_format->get_par()->build( in, 0, NULL, NULL, level );
    //g_object_unref( srgbimg );
    msg = std::string("PF::Image::export_merged(") + filename + "), srgbimg";
    PF_UNREF( srgbimg, msg.c_str() );
    
    vips_image_write_to_file( outimg, filename.c_str() );
    //g_object_unref( outimg );
    msg = std::string("PF::Image::export_merged(") + filename + "), outimg";
    PF_UNREF( outimg, msg.c_str() );
    delete view;
    return true;
  } else {
    return false;
  }
}
