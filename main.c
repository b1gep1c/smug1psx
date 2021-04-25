// From ../psyq/addons/graphics/MESH/RMESH/TUTO0.C :
// 
 /*		   PSX screen coordinate system 
 *
 *                           Z+
 *                          /
 *                         /
 *                        +------X+
 *                       /|
 *                      / |
 *                     /  Y+
 *                   eye		*/

#include <sys/types.h>
#include <stdio.h>
#include <libgte.h>
#include <libetc.h>
#include <libgpu.h>
#include <libapi.h>     // API header, has InitPAD() and StartPAD() defs
// Sound system
#include <libsnd.h>
#include <libspu.h>

#define VMODE 0						// 0 NTSC 1 PAL

#define SCREENXRES 320
#define SCREENYRES 240

#define CENTERX SCREENXRES/2    	// Center of screen on x 
#define CENTERY SCREENYRES/2    	// Center of screen on y

#define MARGINX 90					// margins for text display
#define MARGINY 32

#define FONTSIZE 8 * 7				// Text Field Height

#define OTLEN 8						//ordering table len

DISPENV disp[2];
DRAWENV draw[2];

u_long ot[2][OTLEN];				// double ordering table of length 8 * 32 = 256 bits / 32 bytes

char primbuff[2][32768] = {1};		// pointer to the next primitive in primbuff. Initially, points to the first bit of primbuff[0]
char *nextpri;

int pos_y;
int pos_x;
int speed=1;
int dirY;
int dirX;
int bounce;
int fired;

// Pad stuff (omit when using PSn00bSDK)
#define PAD_SELECT      1
#define PAD_L3          2
#define PAD_R3          4
#define PAD_START       8
#define PAD_UP          16
#define PAD_RIGHT       32
#define PAD_DOWN        64
#define PAD_LEFT        128
#define PAD_L2          256
#define PAD_R2          512
#define PAD_L1          1024
#define PAD_R1          2048
#define PAD_TRIANGLE    4096
#define PAD_CIRCLE      8192
#define PAD_CROSS       16384
#define PAD_SQUARE      32768

typedef struct _PADTYPE{
	unsigned char 	stat;
	unsigned char 	len:4;
	unsigned char 	type:4;
	unsigned short 	btn;
	unsigned char	rs_x,rs_y;
    unsigned char	ls_x,ls_y;
} PADTYPE;

// pad buffer arrays
u_char padbuff[2][34];


short db=0;							// index of which buffer is used, values 0, 1

// Sound stuff

#define MALLOC_MAX 3            // Max number of time we can call SpuMalloc

//~ // convert Little endian to Big endian
#define SWAP_ENDIAN32(x) (((x)>>24) | (((x)>>8) & 0xFF00) | (((x)<<8) & 0x00FF0000) | ((x)<<24))

typedef struct VAGheader{		// All the values in this header must be big endian
        char id[4];			    // VAGp         4 bytes -> 1 char * 4
        unsigned int version;          // 4 bytes
        unsigned int reserved;         // 4 bytes
        unsigned int dataSize;         // (in bytes) 4 bytes
        unsigned int samplingFrequency;// 4 bytes
        char  reserved2[12];    // 12 bytes -> 1 char * 12
        char  name[16];         // 16 bytes -> 1 char * 16
        // Waveform data after that
}VAGhdr;

SpuCommonAttr commonAttributes;          // structure for changing common voice attributes
SpuVoiceAttr  voiceAttributes ;          // structure for changing individual voice attributes

u_long hello_spu_address;                  // address allocated in memory for first sound file
u_long poly_spu_address;                 // address allocated in memory for second sound file

// DEBUG : these allow printing values for debugging

u_long hello_spu_start_address;                
u_long hello_get_start_addr;
u_long hello_transSize;                            

u_long poly_spu_start_address;                
u_long poly_get_start_addr;
u_long poly_transSize;                            

#define HELLO SPU_0CH                   // Play first vag on channel 0
#define POLY SPU_2CH                    // Play second vag on channel 2

// Memory management table ; allow MALLOC_MAX calls to SpuMalloc() - ibref47.pdf p.1044
char spu_malloc_rec[SPU_MALLOC_RECSIZ * (2 + MALLOC_MAX+1)]; 

// VAG files

// We're using GrumpyCoder's Nugget wrapper to compile the code with a modern GCC : https://github.com/grumpycoders/pcsx-redux/tree/main/src/mips/psyq
// To include binary files in the exe, add your VAG files to the SRCS variable in Makefile
// and in common.mk, add this rule to include *.vag files :
//
//~ %.o: %.vag
	//~ $(PREFIX)-objcopy -I binary --set-section-alignment .data=4 --rename-section .data=.rodata,alloc,load,readonly,data,contents -O elf32-tradlittlemips -B mips $< $@


// hello.vag - 44100 Khz
extern unsigned char _binary_VAG_hello_vag_start[]; // filename must begin with _binary_ followed by the full path, with . and / replaced, and then suffixed with _ and end with _start[]; or end[];
extern unsigned char _binary_VAG_hello_vag_end[];   // https://discord.com/channels/642647820683444236/663664210525290507/780866265077383189

// poly.vag - 44100 Khz
extern unsigned char _binary_VAG_poly_vag_start[];
extern unsigned char _binary_VAG_poly_vag_end[];


extern unsigned long _binary_TIM_smug64_tim_start[];
int tim_mode;
RECT tim_prect,tim_crect;
int tim_uoffs,tim_voffs;
TIM_IMAGE smug64;

MATRIX identity(int num)           // generate num x num matrix 
{
   int row, col;
   MATRIX matrix;
   
   for (row = 0; row < num; row++)
   {
      for (col = 0; col < num; col++)
      {
         if (row == col)
            matrix.m[row][col] = 4096;
         else
            matrix.m[row][col] = 0;
      }
   }
   return matrix;
}

void initSnd(void){

    SpuInitMalloc(MALLOC_MAX, spu_malloc_rec);                      // Maximum number of blocks, mem. management table address.
    
    commonAttributes.mask = (SPU_COMMON_MVOLL | SPU_COMMON_MVOLR);  // Mask which attributes to set
    commonAttributes.mvol.left  = 0x3fff;                           // Master volume left
    commonAttributes.mvol.right = 0x3fff;                           // see libref47.pdf, p.1058
    
    SpuSetCommonAttr(&commonAttributes);                            // set attributes
    
    SpuSetIRQ(SPU_OFF);
}

u_long sendVAGtoRAM(unsigned int VAG_data_size, unsigned char *VAG_data){
    u_long size;
    
    SpuSetTransferMode(SpuTransByDMA);                              // DMA transfer; can do other processing during transfer
    
    size = SpuWrite (VAG_data + sizeof(VAGhdr), VAG_data_size);     // transfer VAG_data_size bytes from VAG_data  address to sound buffer
    
    SpuIsTransferCompleted (SPU_TRANSFER_WAIT);                     // Checks whether transfer is completed and waits for completion

    return size;
}

void setVoiceAttr(unsigned int pitch, long channel, unsigned long soundAddr ){
    
    voiceAttributes.mask=                                   //~ Attributes (bit string, 1 bit per attribute)
    (
      SPU_VOICE_VOLL |
	  SPU_VOICE_VOLR |
	  SPU_VOICE_PITCH |
	  SPU_VOICE_WDSA |
	  SPU_VOICE_ADSR_AMODE |
	  SPU_VOICE_ADSR_SMODE |
	  SPU_VOICE_ADSR_RMODE |
	  SPU_VOICE_ADSR_AR |
	  SPU_VOICE_ADSR_DR |
	  SPU_VOICE_ADSR_SR |
	  SPU_VOICE_ADSR_RR |
	  SPU_VOICE_ADSR_SL
    );
    
    voiceAttributes.voice        = channel;                 //~ Voice (low 24 bits are a bit string, 1 bit per voice )
    
    voiceAttributes.volume.left  = 0x1000;                  //~ Volume 
    voiceAttributes.volume.right = 0x1000;                  //~ Volume
    
    voiceAttributes.pitch        = pitch;                   //~ Interval (set pitch)
    voiceAttributes.addr         = soundAddr;               //~ Waveform data start address
    
    voiceAttributes.a_mode       = SPU_VOICE_LINEARIncN;    //~ Attack rate mode  = Linear Increase - see libref47.pdf p.1091
    voiceAttributes.s_mode       = SPU_VOICE_LINEARIncN;    //~ Sustain rate mode = Linear Increase
    voiceAttributes.r_mode       = SPU_VOICE_LINEARDecN;    //~ Release rate mode = Linear Decrease
    
    voiceAttributes.ar           = 0x0;                     //~ Attack rate
    voiceAttributes.dr           = 0x0;                     //~ Decay rate
    voiceAttributes.rr           = 0x0;                     //~ Release rate
    voiceAttributes.sr           = 0x0;                     //~ Sustain rate
    voiceAttributes.sl           = 0xf;                     //~ Sustain level

    SpuSetVoiceAttr(&voiceAttributes);                      // set attributes
    
}

void playSFX(unsigned long fx){
    SpuSetKey(SpuOn, fx); 
}


void LoadTexture(u_long * tim, TIM_IMAGE * tparam){		// This part is from Lameguy64's tutorial series : lameguy64.net/svn/pstutorials/chapter1/3-textures.html login/pw: annoyingmou{
		OpenTIM(tim);                                   // Open the tim binary data, feed it the address of the data in memory
		ReadTIM(tparam);                                // This read the header of the TIM data and sets the corresponding members of the TIM_IMAGE structure
		
        LoadImage(tparam->prect, tparam->paddr);        // Transfer the data from memory to VRAM at position prect.x, prect.y
		DrawSync(0);                                    // Wait for the drawing to end
		
		if (tparam->mode & 0x8){ // check 4th bit       // If 4th bit == 1, TIM has a CLUT
			LoadImage(tparam->crect, tparam->caddr);    // Load it to VRAM at position crect.x, crect.y
			DrawSync(0);                                // Wait for drawing to end
	}
}

void LoadSchesiss(void)
{

	LoadTexture((u_long*)_binary_TIM_smug64_tim_start,&smug64);
	
	tim_prect = *smug64.prect;
	tim_crect = *smug64.crect;
	tim_mode = smug64.mode;
}

void init(void)
{
	ResetGraph(0);
	//initialize and set up the GTE
	InitGeom();
	SetGeomOffset(CENTERX,CENTERY);
	SetGeomScreen(CENTERX);

	SetDefDispEnv(&disp[0], 0, 0, SCREENXRES, SCREENYRES);
	SetDefDispEnv(&disp[1], 0, SCREENYRES,SCREENXRES,SCREENYRES);

	SetDefDrawEnv(&draw[0], 0, SCREENYRES,SCREENXRES,SCREENYRES);
	SetDefDrawEnv(&draw[1], 0, 0, SCREENXRES, SCREENYRES);

	setRGB0(&draw[0], 64, 64, 64);
	setRGB0(&draw[1], 64, 64, 64);

	draw[0].isbg = 1;
    draw[1].isbg = 1;

	LoadSchesiss();

	draw[0].tpage = getTPage( tim_mode&0x3, 0, tim_prect.x, tim_prect.y );
    draw[1].tpage = getTPage( tim_mode&0x3, 0, tim_prect.x, tim_prect.y );
	
	PutDispEnv(&disp[db]);
    PutDrawEnv(&draw[db]);

	FntLoad(960, 0);
	nextpri=primbuff[0];
    FntOpen(MARGINX, SCREENYRES - MARGINY - FONTSIZE, SCREENXRES - MARGINX * 2, FONTSIZE, 1, 280 );

	// Copy the TIM coordinates
    tim_prect   = *smug64.prect;
    tim_crect   = *smug64.crect;
    tim_mode    = smug64.mode;

	//init pad
	InitPAD( padbuff[0],34,padbuff[1],34);
	//begin polling
	StartPAD();
	// To avoid VSync Timeout error, may not be defined in PsyQ
    ChangeClearPAD( 1 );

	pos_y=5;		//pos_y and pos_x are different form one another
	pos_x=10; //to make it start at an interesting angle

	dirY=1;
	dirX=1;
	bounce=0;
	fired=0;

}

void display(void)
{
	DrawSync(0);
	VSync(0);
	
	PutDispEnv(&disp[db]);
	PutDrawEnv(&draw[db]);

	SetDispMask(1);
	
	DrawOTag(ot[db] + OTLEN -1);

	db = !db;
	nextpri=primbuff[db];
}
int main(void)
{
	SPRT *sprt;
	PADTYPE *pad;
	init();

	const VAGhdr * HellofileHeader = (VAGhdr *) _binary_VAG_hello_vag_start;   // get header of first VAG file
    const VAGhdr * PolyfileHeader = (VAGhdr *) _binary_VAG_poly_vag_start;   // get header of second VAG file

	unsigned int Hellopitch =   (SWAP_ENDIAN32(HellofileHeader->samplingFrequency) << 12) / 44100L; 
    unsigned int Polypitch =   (SWAP_ENDIAN32(PolyfileHeader->samplingFrequency) << 12) / 44100L; 

	SpuInit();                                                                            // Initialize SPU. Called only once.
    
    initSnd();

	// First VAG
    
    hello_spu_address   = SpuMalloc(SWAP_ENDIAN32(HellofileHeader->dataSize));                // Allocate an area of dataSize bytes in the sound buffer. 
    
    hello_spu_start_address = SpuSetTransferStartAddr(hello_spu_address);                         // Sets a starting address in the sound buffer
    
    hello_get_start_addr    = SpuGetTransferStartAddr();                                        // SpuGetTransferStartAddr() returns current sound buffer transfer start address.
    
    hello_transSize         = sendVAGtoRAM(SWAP_ENDIAN32(HellofileHeader->dataSize), _binary_VAG_hello_vag_start);
    
     // First VAG
    
    poly_spu_address   = SpuMalloc(SWAP_ENDIAN32(PolyfileHeader->dataSize));                // Allocate an area of dataSize bytes in the sound buffer. 
    
    poly_spu_start_address = SpuSetTransferStartAddr(poly_spu_address);                         // Sets a starting address in the sound buffer
    
    poly_get_start_addr    = SpuGetTransferStartAddr();                                        // SpuGetTransferStartAddr() returns current sound buffer transfer start address.
    
    poly_transSize         = sendVAGtoRAM(SWAP_ENDIAN32(PolyfileHeader->dataSize), _binary_VAG_poly_vag_start);
    
    
    // set VAG to channel 
    
    setVoiceAttr(Hellopitch, HELLO, hello_spu_address); // SPU_0CH == hello
    
    setVoiceAttr(Polypitch, POLY, poly_spu_address);  // SPU_2CH == poly

	short firedO=0;
	
	while(1)
	{
		pad = (PADTYPE*)padbuff[0];
		if (pad->stat == 0)
		{
			if( !(pad->btn&PAD_START) && fired==0) //SOMETHING FUN TO DO: FIND HOW TO MAKE START BUTTON NOT GET POLLED EVERY FRAME
			{							//IN OTHER WORDS: HOW TO MAKE THE START BUTTON INPUT LESS SPAMMY
				bounce=!bounce;
				fired=1;
			}
			if ((pad->btn&PAD_START) && fired==1)
			{
				fired=0;
			}
			// Only parse when a digital pad, 
            // dual-analog and dual-shock is connected
            if( ( pad->type == 0x4 ) || 
                ( pad->type == 0x5 ) || 
                ( pad->type == 0x7 ) ||
				(bounce==0)				)
            {
				 if( !(pad->btn&PAD_UP) )            // test UP
                {
                    pos_y--;
					dirY=-1;
                }
                else if( !(pad->btn&PAD_DOWN) )       // test DOWN
                {
                    pos_y++;
					dirY=1;
                }
                if( !(pad->btn&PAD_LEFT) )          // test LEFT
                {
                    pos_x--;
					dirX=-1;
                }
                else if( !(pad->btn&PAD_RIGHT) )    // test RIGHT
                {
                    pos_x++;
					dirX=1;
                }
				if (!(pad->btn&PAD_CIRCLE)&& firedO==0)
				{
					playSFX(POLY);      // Play second VAG
					firedO=1;
				}
				else if ((pad->btn&PAD_CIRCLE)&& firedO==1)
				{
					firedO=0;
				}
			}
		}
		ClearOTagR(ot[db], OTLEN);
		sprt = (SPRT*)nextpri;

		setSprt(sprt);
		setXY0(sprt,pos_x,pos_y);
		setWH(sprt, 64, 64); 
		setUV0(sprt,                    // Set UV coordinates
            tim_uoffs, 
            tim_voffs);
        setClut(sprt,                   // Set CLUT coordinates to sprite
            tim_crect.x,
            tim_crect.y);
        setRGB0(sprt,                   // Set primitive color
            128, 128, 128);
        addPrim(ot[db], sprt);          // Sort primitive to OT
		nextpri += sizeof(SPRT);        // Advance next primitive address

		FntPrint("ver:%d\nhor:%d\ndirX:%d\ndirY:%d\nSMUG BOUNCE:%d\n",pos_y,pos_x,dirX,dirY,bounce);
		if (!bounce || 
			!(pad->btn&PAD_UP) ||
			!(pad->btn&PAD_DOWN) ||
			!(pad->btn&PAD_LEFT) ||
			!(pad->btn&PAD_RIGHT)
		)
		{
			FntPrint("MANUAL OVERRIDE\n OF THE SMUG");
		}
    
		if (bounce && 
			(pad->btn&PAD_UP) &&
			(pad->btn&PAD_DOWN) &&
			(pad->btn&PAD_LEFT) &&
			(pad->btn&PAD_RIGHT)
		)
		{
			if ((pos_y>233)||(pos_y<-60)||(pos_x>320)||(pos_x<-64))
			{
				FntPrint("WTF? WHERE AM I?\nCOMING HOME");
			}
		}                   
    
        FntFlush(-1);
        if (bounce==1)
		{
			//dvd bouncing below
			pos_y+=speed*dirY;
			if (pos_y>=176){
				dirY=-1;
			}
			if (pos_y<=0){
			dirY=1;}
			
			pos_x+=speed*dirX;
			if (pos_x>=256){
				dirX=-1;
			}
			if (pos_x<=0){
			dirX=1;}
		}
        display();
	}
	return 0;
}