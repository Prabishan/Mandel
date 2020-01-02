//Shrestha, Prabishan R.
//prs3077
//10/09/2019
#include "bitmap.h"
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>

int iteration_to_color( int i, int max );
int iterations_at_point( double x, double y, int max );
void* compute_image( void *ci_args ); //thread function to compute Mandelbrot image
pthread_mutex_t lock;
int temp = 0; // global temp variable created so threads can accesses it

//struct created to pass in each created thread
struct compute_image_struct {
	struct bitmap *bm;
	double xmin;
	double xmax;
	double ymin;
	double ymax;
	int max;
	int width;
	int height;
	int tHeight;
};

void show_help()
{
	printf("Use: mandel [options]\n");
	printf("Where options are:\n");
	printf("-m <max>    The maximum number of iterations per point. (default=1000)\n");
	printf("-x <coord>  X coordinate of image center point. (default=0)\n");
	printf("-y <coord>  Y coordinate of image center point. (default=0)\n");
	printf("-s <scale>  Scale of the image in Mandlebrot coordinates. (default=4)\n");
	printf("-W <pixels> Width of the image in pixels. (default=500)\n");
	printf("-H <pixels> Height of the image in pixels. (default=500)\n");
	printf("-o <file>   Set output file. (default=mandel.bmp)\n");
	printf("-n <thread> The number of threads to run. (default=1)\n"); // -n allows the user to specify number of threads
	printf("-h          Show this help text.\n");
	printf("\nSome examples are:\n");
	printf("mandel -x -0.5 -y -0.5 -s 0.2\n");
	printf("mandel -x -.38 -y -.665 -s .05 -m 100\n");
	printf("mandel -x 0.286932 -y 0.014287 -s .0005 -m 1000\n\n");
}

int main( int argc, char *argv[] )
{
	char c;
	int i;
	double total_time; 		// To keep track of 
	struct timeval tic,toc; // Exection time
	gettimeofday(&tic,NULL); //Captures current system time and stores it in tic

	// These are the default configuration values used
	// if no command line arguments are given.
	const char *outfile = "mandel.bmp";
	double xcenter = 0;
	double ycenter = 0;
	double scale = 4;
	int    image_width = 500;
	int    image_height = 500;
	int    max = 1000;
	int    num_thread = 1; // Default number of threads to run is 1

	// For each command line argument given,
	// override the appropriate configuration value.

	while((c = getopt(argc,argv,"x:y:s:W:H:m:o:n:h"))!=-1) {
		switch(c) {
			case 'x':
				xcenter = atof(optarg);
				break;
			case 'y':
				ycenter = atof(optarg);
				break;
			case 's':
				scale = atof(optarg);
				break;
			case 'W':
				image_width = atoi(optarg);
				break;
			case 'H':
				image_height = atoi(optarg);
				break;
			case 'm':
				max = atoi(optarg);
				break;
			case 'o':
				outfile = optarg;
				break;
			case 'n':
				num_thread = atoi(optarg);
				break;
			case 'h':
				show_help();
				exit(1);
				break;
		}
	}

	// Display the configuration of the image.
	printf("mandel: x=%lf y=%lf scale=%lf max=%d numThread = %d outfile=%s\n",xcenter,ycenter,scale,max,num_thread,outfile);

	// Create a bitmap of the appropriate size.
	struct bitmap *bm = bitmap_create(image_width,image_height);

	// Fill it with a dark blue, for debugging
	bitmap_reset(bm,MAKE_RGBA(0,0,255,0));
	
	// Dynamic memory allocated to struct to store computed value from command line configuration values 
	struct compute_image_struct *ci_args = (struct compute_image_struct*) malloc(sizeof(struct compute_image_struct));
	ci_args->bm = bm;
	ci_args->xmin = xcenter-scale; 
	ci_args->xmax = xcenter+scale;
	ci_args->ymin = ycenter-scale;
	ci_args->ymax = ycenter+scale;
	ci_args->max = max;
	ci_args->tHeight = image_height / num_thread; // Size of image height that each thread is going to compute
	pthread_t tid[num_thread]; // Thread ID array
	for( i = 0 ; i < num_thread ; i++){
		//Thread created to compute Mandelbrot image
		pthread_create(&tid[i],NULL, compute_image, (void*)ci_args);
	}
	//Waits until all thread has computed Mandelbrot image
	for( i = 0 ; i <num_thread ; i++){
		pthread_join(tid[i],NULL);
	}

	// Save the image in the stated file.
	if(!bitmap_save(ci_args->bm,outfile)) {
		fprintf(stderr,"mandel: couldn't write to %s: %s\n",outfile,strerror(errno));
		return 1;
	}
	gettimeofday(&toc,NULL); //Captures current system time and stores it in toc
	// finding the difference between start and end time and then converting it into seconds
	// which gives the total runtime time of the program 
	total_time = ((double) (toc.tv_usec - tic.tv_usec))/ 1000000 + (double) (toc.tv_sec - tic.tv_sec);
	printf("Time taken to execute: %fsec\n", total_time);
	return 0;
}

/*
Compute an entire Mandelbrot image, writing each point to the given bitmap.
Scale the image to the range (xmin-xmax,ymin-ymax), limiting iterations to "max"
*/

void* compute_image( void *ci_args )
{
	int i,j,start_position ,thread_height;
	// Struct created to access to each values of compute_image_struct
	struct compute_image_struct *cis = (struct compute_image_struct*) ci_args ;
	int width = bitmap_width(cis->bm);
	int height = bitmap_height(cis->bm);
	
	// Each thread access the values and changes the global variable temp
	// turn by turn. First thread to get here locks this critical region
	pthread_mutex_lock(&lock);
	// start_position stores the start pixel for that thread 		
	start_position = temp; 
	// thread_height stores the end pixel for that thread
	// and adds one so that it doesn't miss pixel at the end
	thread_height = start_position + cis->tHeight+1; 
	// Global variable "temp" stores value of end pixel of this thread 
	// which later becomes start pixel for another thread
	temp = thread_height;  
	// Thread unlocks this critical region for another thread
	pthread_mutex_unlock(&lock); 
	// For every pixel in the image...
	for(j=start_position;j<thread_height;j++) {
		for(i=0;i<width;i++) {
			// Determine the point in x,y space for that pixel.
			double x = cis->xmin + i*(cis->xmax-cis->xmin)/width;
			double y = cis->ymin + j*(cis->ymax-cis->ymin)/height;

			// Compute the iterations at that point.
			int iters = iterations_at_point(x,y,cis->max);

			// Set the pixel in the bitmap.
			bitmap_set(cis->bm,i,j,iters);
		}
	}
	pthread_exit(0);	
}

/*
Return the number of iterations at point x, y
in the Mandelbrot space, up to a maximum of max.
*/

int iterations_at_point( double x, double y, int max )
{
	double x0 = x;
	double y0 = y;

	int iter = 0;

	while( (x*x + y*y <= 4) && iter < max ) {

		double xt = x*x - y*y + x0;
		double yt = 2*x*y + y0;

		x = xt;
		y = yt;

		iter++;
	}

	return iteration_to_color(iter,max);
}

/*
Convert a iteration number to an RGBA color.
Here, we just scale to gray with a maximum of imax.
Modify this function to make more interesting colors.
*/

int iteration_to_color( int i, int max )
{
	int gray = 255*i/max;
	return MAKE_RGBA(gray,gray,gray,0);
}




