// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
//
// This code is public domain
// (but note, once linked against the led-matrix library, this is
// covered by the GPL v2)

#include "led-matrix.h"
#include "threaded-canvas-manipulator.h"
#include "transformer.h"
#include "graphics.h"

#include <assert.h>
#include <getopt.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <algorithm>

#include <string.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/time.h>

using std::min;
using std::max;

using namespace rgb_matrix;

class Timer {
private:

    timeval startTime;

public:

    void start(){
        gettimeofday(&startTime, NULL);
    }

    double stop(){
        timeval endTime;
        long seconds, useconds;
        double duration;

        gettimeofday(&endTime, NULL);

        seconds  = endTime.tv_sec  - startTime.tv_sec;
        useconds = endTime.tv_usec - startTime.tv_usec;

        duration = seconds + useconds/1000000.0;

        return duration;
    }

    static void printTime(double duration){
        printf("%5.6f seconds\n", duration);
    }
};

//Setup receiving server and populate screen pixels
class ScreenPixelatorServer : public ThreadedCanvasManipulator {
	int port_;
public:
  ScreenPixelatorServer(Canvas *m, int port) : ThreadedCanvasManipulator(m) {
	  port_ = port;
  }
  void Run() {
     int sockfd, newsockfd, portno = port_, clilen;
     struct sockaddr_in serv_addr, cli_addr;
     int data;

     printf( "using port #%d\n", portno );
    
     sockfd = socket(AF_INET, SOCK_STREAM, 0);
     if (sockfd < 0) 
         error( const_cast<char *>("ERROR opening socket") );
     bzero((char *) &serv_addr, sizeof(serv_addr));

     serv_addr.sin_family = AF_INET;
     serv_addr.sin_addr.s_addr = INADDR_ANY;
     serv_addr.sin_port = htons( portno );
     if (bind(sockfd, (struct sockaddr *) &serv_addr,
              sizeof(serv_addr)) < 0) 
       error( const_cast<char *>( "ERROR on binding" ) );
     listen(sockfd,5);
     clilen = sizeof(cli_addr);
  
     //--- infinite wait on a connection ---
     while ( running() ) {
        printf( "waiting for new client...\n" );
        if ( ( newsockfd = accept( sockfd, (struct sockaddr *) &cli_addr, (socklen_t*) &clilen) ) < 0 )
            error( const_cast<char *>("ERROR on accept") );
        printf( "opened new communication with client\n" );
		printf( "listening for new pixelpackets\n");
		data = 0;
        //while ( 1 ) {
             //---- wait for a number from client ---
			 while( handlePackets( newsockfd ) == 0 && running()) {
				//data = getLength( newsockfd );
			 }
             //printf( "need to read %d more bytes\n", data - 4);
             //if ( data < 0 ) 
                //break;
                
             //getBitData( newsockfd, data - 4 );
             //--- send new data back --- 
             //printf( "done printing pixeldata:\n");
			 //data = 0;
        //}
        close( newsockfd );

        //--- if -2 sent by client, we can quit ---
        if ( data == -2 )
          break;
     }
  }

private:
  //RGBMatrix *const matrix_;
  //FrameCanvas *off_screen_canvas_;
  void error( char *msg ) {
    perror(  msg );
    exit(1);
  }
  
  int getLength( int sockfd ) {
    char buffer[4];
    ssize_t bytes_read;
    bytes_read = 0;
    while(bytes_read < 4) {
	  bytes_read = recv(sockfd, buffer, sizeof(buffer), 0);
    }  
    return (buffer[0] << 24) + (buffer[1] << 16) + (buffer[2] << 8) + buffer[3];
  }

  void getBitData( int sockfd, int length ) {
	Timer timer = Timer();
	timer.start();
    char buffer[length];
    ssize_t bytes_read;
    bytes_read = 0;

    while(bytes_read < length) {
	  bytes_read = recv(sockfd, buffer, sizeof(buffer), 0);
    }
	const int width = canvas()->width();
	const int height = canvas()->height();
    for(int y = 0; y < height; ++y) {
	  for(int x = 0; x < width; ++x) {
		canvas()->SetPixel(x, y, (int)buffer[y * 128 * 3 + x * 3], (int)buffer[y * 128 * 3 + x * 3 + 1], (int)buffer[y * 128 * 3 + x * 3 + 2]);
	  }
    }
	double duration = timer.stop();
	timer.printTime(duration);
  }
  
  int handlePackets( int sockfd ) {	  
	const int width = canvas()->width();
	const int height = canvas()->height();
	char buffer[4];
	int n;
    ssize_t bytes_read;
    bytes_read = 0;
    while(bytes_read < 4) {
	  bytes_read = recv(sockfd, buffer, sizeof(buffer), 0);
    }  
	
    int to_read = (buffer[0] << 24) + (buffer[1] << 16) + (buffer[2] << 8) + buffer[3];
	char pixels[to_read];
	
	if ( ( n = read(sockfd, pixels, to_read) ) < 0 ) {
		error( const_cast<char *>( "ERROR reading from socket") );
		return -1;
	}
	for(int y = 0; y < height; ++y) {
	  for(int x = 0; x < width; ++x) {
		canvas()->SetPixel(x, y, (int)pixels[y * 128 * 3 + x * 3], (int)pixels[y * 128 * 3 + x * 3 + 1], (int)pixels[y * 128 * 3 + x * 3 + 2]);
	  }
    }
	return 0;
  }
};

static int usage(const char *progname) {
  fprintf(stderr, "usage: %s <options> -D <demo-nr> [optional parameter]\n",
          progname);
  fprintf(stderr, "Options:\n"
          "\t-r <rows>     : Panel rows. '16' for 16x32 (1:8 multiplexing),\n"
	  "\t                '32' for 32x32 (1:16), '8' for 1:4 multiplexing; 64 for 1:32 multiplexing. "
          "Default: 16\n"
          "\t-P <parallel> : For Plus-models or RPi2: parallel chains. 1..3. "
          "Default: 1\n"
          "\t-c <chained>  : Daisy-chained boards. Default: 1. (set to 8)\n"
		  "\t-T            : Transform to rectangular display, composed of 8 times 32x16 to form a 128x32 matrix\n"
          "\t-p <pwm-bits> : Bits used for PWM. Something between 1..11\n"
		  "\t-s            : Run as Server\receiver for ScreenPixelator\n"
		  "\t-i            : Port used for ScreenPixelatorServer default: 29960\n"
          "\t-l            : Don't do luminance correction (CIE1931)\n"
          "\t-d            : run as daemon. Use this when starting in\n"
          "\t                /etc/init.d, but also when running without\n"
          "\t                terminal (e.g. cron).\n"          
          "\t-b <brightnes>: Sets brightness percent. Default: 100.\n"
          "\t-R <rotation> : Sets the rotation of matrix. Allowed: 0, 90, 180, 270. Default: 0.\n");
  return 1;
}

int main(int argc, char *argv[]) {
  GPIO io;
  bool as_daemon = false;
  int rows = 32;
  int chain = 1;
  int parallel = 1;
  int pwm_bits = -1;
  int brightness = 100;
  int rotation = 0;
  bool rect_display = true;
  bool do_luminance_correct = true;
  int port = 29960;
  int runtime_seconds = 0;
  

  int opt;
  while ((opt = getopt(argc, argv, "dl:r:P:c:p:i:b:TR:")) != -1) {
    switch (opt) {
	case 'i':
      port = atoi(optarg);
      break;

    case 'd':
      as_daemon = true;
      break;

    case 'r':
      rows = atoi(optarg);
      break;

    case 'P':
      parallel = atoi(optarg);
      break;

    case 'c':
      chain = atoi(optarg);
      break;

    case 'p':
      pwm_bits = atoi(optarg);
      break;

    case 'b':
      brightness = atoi(optarg);
      break;

    case 'l':
      do_luminance_correct = !do_luminance_correct;
      break;

    case 'T':
      // The rectangular display assumes a chain of eight displays with 32x16
      chain = 8;
      rows = 16;
      rect_display = true;
      break;

    case 'R':
      rotation = atoi(optarg);
      break;

    default: /* '?' */
      return usage(argv[0]);
    }
  }

  if (getuid() != 0) {
    fprintf(stderr, "Must run as root to be able to access /dev/mem\n"
            "Prepend 'sudo' to the command:\n\tsudo %s ...\n", argv[0]);
    return 1;
  }

  if (rows != 8 && rows != 16 && rows != 32 && rows != 64) {
    fprintf(stderr, "Rows can one of 8, 16, 32 or 64 "
            "for 1:4, 1:8, 1:16 and 1:32 multiplexing respectively.\n");
    return 1;
  }

  if (chain < 1) {
    fprintf(stderr, "Chain outside usable range\n");
    return 1;
  }
  if (chain > 8) {
    fprintf(stderr, "That is a long chain. Expect some flicker.\n");
  }
  if (parallel < 1 || parallel > 3) {
    fprintf(stderr, "Parallel outside usable range.\n");
    return 1;
  }

  if (brightness < 1 || brightness > 100) {
    fprintf(stderr, "Brightness is outside usable range.\n");
    return 1;
  }

  if (rotation % 90 != 0) {
    fprintf(stderr, "Rotation %d not allowed! Only 0, 90, 180 and 270 are possible.\n", rotation);
    return 1;
  }

  // Initialize GPIO pins. This might fail when we don't have permissions.
  if (!io.Init())
    return 1;

  // Start daemon before we start any threads.
  if (as_daemon) {
    if (fork() != 0)
      return 0;
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
  }

  // The matrix, our 'frame buffer' and display updater.
  RGBMatrix *matrix = new RGBMatrix(&io, rows, chain, parallel);
  matrix->set_luminance_correct(do_luminance_correct);
  matrix->SetBrightness(brightness);
  if (pwm_bits >= 0 && !matrix->SetPWMBits(pwm_bits)) {
    fprintf(stderr, "Invalid range of pwm-bits\n");
    return 1;
  }

  LinkedTransformer *transformer = new LinkedTransformer();
  matrix->SetTransformer(transformer);

  if (rect_display) {
    // Mapping the coordinates of a 32x128 display mapped to a square of 64x64
    transformer->AddTransformer(new RectangleTransformer());
  }

  if (rotation > 0) {
    transformer->AddTransformer(new RotateTransformer(rotation));
  }

  Canvas *canvas = matrix;

  // The ThreadedCanvasManipulator objects are filling
  // the matrix continuously.
  ThreadedCanvasManipulator *image_gen = NULL;
  
  image_gen = new ScreenPixelatorServer(canvas, port);

  if (image_gen == NULL)
    return usage(argv[0]);

  // Image generating demo is crated. Now start the thread.
  image_gen->Start();

  // Now, the image genreation runs in the background. We can do arbitrary
  // things here in parallel. In this demo, we're essentially just
  // waiting for one of the conditions to exit.
  if (as_daemon) {
    sleep(runtime_seconds > 0 ? runtime_seconds : INT_MAX);
  } else if (runtime_seconds > 0) {
    sleep(runtime_seconds);
  } else {
    // Things are set up. Just wait for <RETURN> to be pressed.
    printf("Press <RETURN> to exit and reset LEDs\n");
    getchar();
  }

  // Stop image generating thread.
  delete image_gen;
  delete canvas;

  transformer->DeleteTransformers();
  delete transformer;

  return 0;
}
