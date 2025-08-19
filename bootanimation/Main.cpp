#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <vector>
#include <string>
#include "Video.h"
#include "Audio.h"
#include "zip.h"
#include <Magick++.h>

using namespace Magick;

bool audio_present = false;
int audio_sample_rate = 0;
int audio_channels = 0;
uint64_t audio_samples = 0;
int16_t* audio_buffer = nullptr;

#define ANIM_PATH_MAX 255
#define STR(x)   #x
#define STRTO(x) STR(x)

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

static const int TEXT_CENTER_VALUE = INT_MAX;
static const int TEXT_MISSING_VALUE = INT_MIN;
static const int ANIM_ENTRY_NAME_MAX = ANIM_PATH_MAX + 1;
static constexpr size_t TEXT_POS_LEN_MAX = 16;

typedef struct {
    uint8_t R;
    uint8_t G;
    uint8_t B;
} RGBColor;

typedef struct {
    int w, h, x, y;
} Position;

//desc.txt entries..................
//Based on documentation from:
//https://android.googlesource.com/platform/frameworks/base/+/master/cmds/bootanimation/FORMAT.md?pli=1#

typedef struct {
	int width;            //width of the animation
	int height;           //height of the animation
	int fps;              //frames per second
	int progress;         //progress of the animation, unsupported currently
} DescHdr;

typedef enum {
    DESC_TYPE_PLAY = 0,
	DESC_TYPE_COMPLETION,
    DESC_TYPE_FADE
} DescType;

typedef struct {
	DescType type;          //type of entry
    int count;              //how many times to play the animation
	int pause;  		    //number of FRAMES to delay after this part ends
    std::string path;       //folder of frames
    int fade_frames;        //fade frames when completed
	int clock1;            //clock1, unsupported currently
	int clock2;            //clock2, unsupported currently
    bool use_dyncol;
    bool post_dyncol;
    RGBColor bkg_color;
	int duration_ms; //duration in milliseconds
} DescEntry;

typedef struct {
    DescHdr hdr;           //header of the desc.txt
    int dyncol_start; //start of dynamic coloring
    int dyncol_end;   //end of dynamic coloring, in milliseconds
	RGBColor dyncol_start_colors[4]; //start colors for dynamic coloring
    RGBColor dyncol_end_colors[4];   //end colors for dynamic coloring
	std::string dyncol_part_name; //name of the part to apply dynamic coloring to
	bool dyncol_enabled; //is dynamic coloring enabled
    std::vector<DescEntry> entries; //entries in the desc.txt
} DescTxt;

//zip entries
std::vector<std::string> zipEntries;

DescTxt descTxt; //desc.txt contents

zip_t* zipArchive = NULL;
Audio* audio = NULL;
std::string outFileName = "bootanimation.mp4";
std::string dynamicColoringPartName;

int re = 0, ge = 0, be = 0;
float dyn_percentage = 0;

uint32_t audio_total_ms = 0;

int FpsToMs(int fps)
{
    if (fps <= 0) return 0;
    return 1000 / fps;
}

/*
Dynamic coloring is a render mode that draws the boot animation using a color transition. In this mode, instead of directly rendering the PNG images, it treats the R, G, B, A channels of input images as area masks of dynamic colors, which interpolates between start and end colors based on animation progression.

To enable it, add the following line as the second line of desc.txt:

dynamic_colors PATH #RGBHEX1 #RGBHEX2 #RGBHEX3 #RGBHEX4
PATH: file path of the part to apply dynamic color transition to. Any part before this part will be rendered in the start colors. Any part after will be rendered in the end colors.
RGBHEX1: the first start color (masked by the R channel), specified as #RRGGBB.
RGBHEX2: the second start color (masked by the G channel), specified as #RRGGBB.
RGBHEX3: the third start color (masked by the B channel), specified as #RRGGBB.
RGBHEX4: the forth start color (masked by the A channel), specified as #RRGGBB.
*/

void ApplyDynamic(uint8_t* in, uint8_t* out, RGBColor R, RGBColor G, RGBColor B, RGBColor A, int width, int height)
{
    int size = width * height * 4;
	int outsize = width * height * 3;
    //parse R
    for (int a = 0, size_out_work = 0; a < size && size_out_work < outsize; a += 4, size_out_work+= 3)
    {
        int ra, ga, ba;
		ra = out[size_out_work] + (in[a] * R.R) / 255;
		ga = out[size_out_work + 1] + (in[a] * R.G) / 255;
		ba = out[size_out_work + 2] + (in[a] * R.B) / 255;
        out[size_out_work] = clampToU8(ra / 255.0);
        out[size_out_work + 1] = clampToU8(ga / 255.0);
		out[size_out_work + 2] = clampToU8(ba / 255.0);
    }
	//parse G
    for (int a = 1, size_out_work = 0; a < size && size_out_work < outsize; a += 4, size_out_work += 3)
    {
        int ra, ga, ba;
        ra = out[size_out_work] + (in[a] * G.R) / 255;
        ga = out[size_out_work + 1] + (in[a] * G.G) / 255;
        ba = out[size_out_work + 2] + (in[a] * G.B) / 255;
        out[size_out_work] = clampToU8(ra / 255.0);
        out[size_out_work + 1] = clampToU8(ga / 255.0);
        out[size_out_work + 2] = clampToU8(ba / 255.0);
    }
	//parse B
    for (int a = 2, size_out_work = 0; a < size && size_out_work < outsize; a += 4, size_out_work += 3)
    {
        int ra, ga, ba;
        ra = out[size_out_work] + (in[a] * B.R) / 255;
        ga = out[size_out_work + 1] + (in[a] * B.G) / 255;
        ba = out[size_out_work + 2] + (in[a] * B.B) / 255;
        out[size_out_work] = clampToU8(ra / 255.0);
        out[size_out_work + 1] = clampToU8(ga / 255.0);
        out[size_out_work + 2] = clampToU8(ba / 255.0);
    }
    //parse A
    for (int a = 3, size_out_work = 0; a < size && size_out_work < outsize; a += 4, size_out_work += 3)
    {
        int ra, ga, ba;
        ra = out[size_out_work] + (in[a] * A.R) / 255;
        ga = out[size_out_work + 1] + (in[a] * A.G) / 255;
        ba = out[size_out_work + 2] + (in[a] * A.B) / 255;
        out[size_out_work] = clampToU8(ra / 255.0);
        out[size_out_work + 1] = clampToU8(ga / 255.0);
        out[size_out_work + 2] = clampToU8(ba / 255.0);
    }
    //currently this is a stupid implementation, so check the R G B A of every pixel in source are equal
    //If so, change the values of R,G,B to the source's ones (e.g. the Powered by Android logo on Pixel 8 Pro bootanim dark)
    for (int a = 0, size_out_work = 0; a < size && size_out_work < outsize; a += 4, size_out_work += 3)
    {
        if (in[a] == in[a + 1] && in[a + 1] == in[a + 2] && in[a + 2] == in[a + 3]) {
            out[size_out_work] = in[a];
            out[size_out_work + 1] = in[a];
            out[size_out_work + 2] = in[a];
		}
    }
}

void FillImage(uint8_t* pRGB, RGBColor color, int w, int h)
{
    int size = w * h * 3; // RGB
    for (int i = 0; i < size; i += 3) {
        pRGB[i] = color.R;
        pRGB[i + 1] = color.G;
        pRGB[i + 2] = color.B;
	}
}

// Sort entries with trim.txt as the first item in its folder
void SortZipEntries()
{
    // First, sort all entries
    std::sort(zipEntries.begin(), zipEntries.end(), [](const std::string& a, const std::string& b) {
        size_t posA = a.find_last_of('/');
        size_t posB = b.find_last_of('/');
        
        // If either path has no directory, sort by full path
        if (posA == std::string::npos || posB == std::string::npos) {
            return a < b;
        }
        
        std::string dirA = a.substr(0, posA);
        std::string dirB = b.substr(0, posB);
        
        // If different directories, sort by directory name
        if (dirA != dirB) {
            return dirA < dirB;
        }
        
        // For same directory, make sure trim.txt comes first
        std::string fileA = a.substr(posA + 1);
        std::string fileB = b.substr(posB + 1);
        
        if (fileA == "trim.txt") return true;
        if (fileB == "trim.txt") return false;
        
        // Otherwise sort by filename
        return a < b;
    });
}


void ProcessZipFile()
{
    zip_int64_t numEntries = zip_get_num_entries(zipArchive, 0);
    if (numEntries < 0) {
		printf("Failed to get number of entries in zip file\n");
        zip_close(zipArchive);
        return;
    }
    for (zip_uint64_t i = 0; i < static_cast<zip_uint64_t>(numEntries); ++i) {
        const char* entryName = zip_get_name(zipArchive, i, 0);
        if (entryName)
            zipEntries.push_back(entryName);
    }
}

// Parse a color represented as an HTML-style 'RRGGBB' string: each pair of
// characters in str is a hex number in [0, 255], which are converted to
// floating point values in the range [0.0, 1.0] and placed in the
// corresponding elements of color.
//
// If the input string isn't valid, parseColor returns false and color is
// left unchanged.
static bool parseColor(const char str[7], RGBColor* color) {
    for (int i = 0; i < 3; i++) {
        int val = 0;
        for (int j = 0; j < 2; j++) {
            val *= 16;
            char c = str[2 * i + j];
            if (c >= '0' && c <= '9') val += c - '0';
            else if (c >= 'A' && c <= 'F') val += (c - 'A') + 10;
            else if (c >= 'a' && c <= 'f') val += (c - 'a') + 10;
            else                           return false;
        }
        if (i==0)
		    color->R = (uint8_t)val;
		else if (i == 1)
			color->G = (uint8_t)val;
		else
			color->B = (uint8_t)val;
    }
    return true;
}

bool parseTextCoord(const char* str, int* dest) {
    if (strcmp("c", str) == 0) {
        *dest = TEXT_CENTER_VALUE;
        return true;
    }
    char* end;
    int val = (int)strtol(str, &end, 0);
    if (end == str || *end != '\0' || val == INT_MAX || val == INT_MIN) {
        return false;
    }
    *dest = val;
    return true;
}

void parsePosition(const char* str1, const char* str2, int* x, int* y) {
    bool success = false;
    if (strlen(str1) == 0) {
        // success = false
    }
    else if (strlen(str2) == 0) {  // we have only one value
        if (parseTextCoord(str1, y)) {
            *x = TEXT_CENTER_VALUE;
            success = true;
        }
    }
    else {
        if (parseTextCoord(str1, x) && parseTextCoord(str2, y)) {
            success = true;
        }
    }
    if (!success) {
        *x = TEXT_MISSING_VALUE;
        *y = TEXT_MISSING_VALUE;
    }
}

//mix colors as a transition between start and end colors
RGBColor mixColorsInt(const RGBColor& color1, const RGBColor& color2, double percentage) {
    RGBColor Out;
    percentage = MAX(0.0, MIN(1.0, percentage));

    int mixedR = static_cast<int>(color1.R * (1.0 - percentage) + color2.R * percentage);
    int mixedG = static_cast<int>(color1.G * (1.0 - percentage) + color2.G * percentage);
    int mixedB = static_cast<int>(color1.B * (1.0 - percentage) + color2.B * percentage);

    mixedR = MAX(0, MIN(255, mixedR));
    mixedG = MAX(0, MIN(255, mixedG));
    mixedB = MAX(0, MIN(255, mixedB));
    
	Out.R = static_cast<uint8_t>(mixedR);
	Out.G = static_cast<uint8_t>(mixedG);
	Out.B = static_cast<uint8_t>(mixedB);

    return Out;
}

RGBColor mixColors(const RGBColor& color1, const RGBColor& color2, float percentage) {
    return mixColorsInt(color1, color2, percentage / 100.0);
}

void ExtractDescTxt()
{
    struct zip_stat st;
    zip_stat_init(&st);
    zip_stat(zipArchive, "desc.txt", 0, &st);

    //Alloc memory for its uncompressed contents
    char* contents = new char[st.size];

    zip_file* f = zip_fopen(zipArchive, "desc.txt", 0);
    zip_fread(f, contents, st.size);
    zip_fclose(f);

    char const* s = contents;
    std::string dynamicColoringPartName = "";
    bool postDynamicColoring = false;
    // Parse the description file
    for (;;) {
        const char* endl = strstr(s, "\n");
        if (endl == nullptr) break;
        std::string line(s, endl - s);
        const char* l = line.c_str();
        int fps = 0;
        int width = 0;
        int height = 0;
        int count = 0;
        int pause = 0;
        int progress = 0;
        int framesToFadeCount = 0;
        int colorTransitionStart = 0;
        int colorTransitionEnd = 0;
        char path[ANIM_ENTRY_NAME_MAX];
        char color[7] = "000000"; // default to black if unspecified
        char clockPos1[TEXT_POS_LEN_MAX + 1] = "";
        char clockPos2[TEXT_POS_LEN_MAX + 1] = "";
        char dynamicColoringPartNameBuffer[ANIM_ENTRY_NAME_MAX];
        char pathType;
        // start colors default to black if unspecified
        char start_color_0[7] = "000000";
        char start_color_1[7] = "000000";
        char start_color_2[7] = "000000";
        char start_color_3[7] = "000000";
        int nextReadPos;
        if (strlen(l) == 0) {
            s = ++endl;
            continue;
        }
        int topLineNumbers = sscanf(l, "%d %d %d %d", &width, &height, &fps, &progress);
        if (topLineNumbers == 3 || topLineNumbers == 4) {
			printf("> width=%d, height=%d, fps=%d, progress=%d\n", width, height, fps, progress);
			descTxt.hdr.width = width;
			descTxt.hdr.height = height;
			descTxt.hdr.fps = fps;
            if (topLineNumbers == 4) {
				descTxt.hdr.progress = progress;
            }
            else {
                descTxt.hdr.progress = 0;
            }
        }
        else if (sscanf(l, "dynamic_colors %" STRTO(ANIM_PATH_MAX) "s #%6s #%6s #%6s #%6s %d %d",
            dynamicColoringPartNameBuffer,
            start_color_0, start_color_1, start_color_2, start_color_3,
            &colorTransitionStart, &colorTransitionEnd)) {
			descTxt.dyncol_enabled = true;
            parseColor(start_color_0, &descTxt.dyncol_start_colors[0]);
            parseColor(start_color_1, &descTxt.dyncol_start_colors[1]);
            parseColor(start_color_2, &descTxt.dyncol_start_colors[2]);
            parseColor(start_color_3, &descTxt.dyncol_start_colors[3]);
			descTxt.dyncol_start = colorTransitionStart;
			descTxt.dyncol_end = colorTransitionEnd;
            descTxt.dyncol_part_name = std::string(dynamicColoringPartNameBuffer);
        }
        else if (sscanf(l, "%c %d %d %" STRTO(ANIM_PATH_MAX) "s%n",
            &pathType, &count, &pause, path, &nextReadPos) >= 4) {
			DescEntry entry;
            switch (pathType) {
                case 'p':
                    entry.type = DESC_TYPE_PLAY;
                    break;
                case 'c':
                    entry.type = DESC_TYPE_COMPLETION;
                    break;
                case 'f':
                    entry.type = DESC_TYPE_FADE;
                    break;
                default:
                    printf("> invalid path type '%c'", pathType);
                    continue; // skip invalid entries
			}
            if (pathType == 'f') {
                sscanf(l + nextReadPos, " %d #%6s %16s %16s", &framesToFadeCount, color, clockPos1,
                    clockPos2);
            }
            else {
                sscanf(l + nextReadPos, " #%6s %16s %16s", color, clockPos1, clockPos2);
            }
            printf("> type=%c, count=%d, pause=%d, path=%s, framesToFadeCount=%d, color=%s, "
                   "clockPos1=%s, clockPos2=%s\n",
                   pathType, count, pause, path, framesToFadeCount, color, clockPos1, clockPos2);
            if (path == dynamicColoringPartName) {
                // Part is specified to use dynamic coloring.
				entry.use_dyncol = true;
                entry.post_dyncol = false;
                postDynamicColoring = true;
            }
            else {
                // Part does not use dynamic coloring.
                entry.use_dyncol = false;
                entry.post_dyncol = postDynamicColoring;
            }
			entry.fade_frames = framesToFadeCount;
			entry.count = count;
            entry.pause = pause;
            entry.path = std::string(path);
            if (!parseColor(color, &entry.bkg_color)) {
                printf("> invalid color '#%s'\n", color);
                entry.bkg_color.R = 0.0f;
                entry.bkg_color.G = 0.0f;
                entry.bkg_color.B = 0.0f;
            }
            parsePosition(clockPos1, clockPos2, &entry.clock1, &entry.clock2);
            entry.duration_ms = 0;
			descTxt.entries.push_back(entry);
        }
        else if (strcmp(l, "$SYSTEM") == 0) {
            printf("> not supported\n");
        }
        s = ++endl;
    }


}

void PasteImage(uint8_t* pRGB, uint8_t* pInRGB, int x, int y, int w, int h)
{
	//paste img at x,y to pRGB
    if (x < 0 || y < 0 || x + w > descTxt.hdr.width || y + h > descTxt.hdr.height) {
        printf("Invalid paste position (%d, %d) with size (%d, %d)\n", x, y, w, h);
        return;
    }
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            int srcIndex = (j * w + i) * 3;
            int destIndex = ((y + j) * descTxt.hdr.width + (x + i)) * 3;
            pRGB[destIndex] = pInRGB[srcIndex];
            pRGB[destIndex + 1] = pInRGB[srcIndex + 1];
            pRGB[destIndex + 2] = pInRGB[srcIndex + 2];
        }
	}
}

void ParseEntry(std::string prefix, Video* video, RGBColor bkg, int pause)
{
    std::vector<Position> pos;
    RGBColor dyn_colors[4];
    bool hasTrim = false;
    int curr_frame = 0;

    std::string last_frame;

    //find last frame in the prefix
    for (const auto& zipEntry : zipEntries) {
        if (zipEntry.find(prefix) == 0 && zipEntry.length() > prefix.length()) {
            if (zipEntry > last_frame && !(zipEntry.find(".txt") != std::string::npos)) {
                last_frame = zipEntry;
            }
        }
    }

    for (const auto& zipEntry : zipEntries) {
        if (zipEntry.find(prefix) == 0 && zipEntry.length() > prefix.length()) {

            int times = 1 + ((zipEntry == last_frame) ? pause : 0);

            uint8_t* file_data = NULL;
            zip_uint64_t file_size = 0;
            zip_stat_t st;

            zip_file* f = zip_fopen(zipArchive, zipEntry.c_str(), 0);
            if (!f) {
                printf("Failed to open file %s in zip\n", zipEntry.c_str());
                continue;
            }

            zip_stat(zipArchive, zipEntry.c_str(), 0, &st);
            file_size = st.size;
            file_data = new uint8_t[file_size];
            zip_fread(f, file_data, file_size);
            zip_fclose(f);

            //if file is trim.txt, parse it
            if (zipEntry.find("trim.txt") != std::string::npos)
            {
                char const* s = (char*)file_data;
                for (;;) {
                    Position entry;
                    const char* endl = strstr(s, "\n");
                    if (endl == nullptr) break;
                    std::string line(s, endl - s);
                    const char* l = line.c_str();
                    if (strlen(l) == 0) {
                        s = ++endl;
                        continue;
                    }
                    int a = sscanf(l, "%dx%d+%d+%d", &entry.w, &entry.h, &entry.x, &entry.y);
                    pos.push_back(entry);
                    s = ++endl;
                }
                hasTrim = true;
            }
            else
            {
                for (int i = 0; i < times; i++)
                {
                    uint8_t* a = video->GetFrameData();
                    FillImage(a, bkg, descTxt.hdr.width, descTxt.hdr.height);
                    try {
                        uint8_t* dec_buffer = NULL;
                        int dec_buffer_size = 0;
                        uint8_t* dec_buffer_dcol = NULL;
                        int dec_buffer_dcol_size = 0;
                        Magick::Blob blob(file_data, file_size);
                        Magick::Image image;
                        int rgb_multi = ((descTxt.dyncol_enabled == true) ? 4 : 3);
                        int rgb_buffer_size = 0;
                        image.read(blob);

                        printf("open frame of %s, size=%d, width=%d, height=%d\n",
                            zipEntry.c_str(), file_size, image.columns(), image.rows());

                        //alloc

                        if (descTxt.dyncol_enabled == true)
                        {
                            dec_buffer = new uint8_t[image.columns() * image.rows() * 4];
                            dec_buffer_size = image.columns() * image.rows() * 4;
                            dec_buffer_dcol = new uint8_t[image.columns() * image.rows() * 3];
                            dec_buffer_dcol_size = image.columns() * image.rows() * 3;
                            const MagickCore::Quantum* pixelsArr = image.getConstPixels(0, 0, image.columns(), image.rows());
                            for (unsigned int i = 0; i < image.columns() * image.rows() * rgb_multi; i += rgb_multi) {
                                for (unsigned int i1 = 0; i1 < 2; i1++) {
                                    dec_buffer[rgb_buffer_size] = pixelsArr[i + 0] * 255 / QuantumRange;
                                    dec_buffer[rgb_buffer_size + 1] = pixelsArr[i + 1] * 255 / QuantumRange;
                                    dec_buffer[rgb_buffer_size + 2] = pixelsArr[i + 2] * 255 / QuantumRange;
                                    dec_buffer[rgb_buffer_size + 3] = pixelsArr[i + 3] * 255 / QuantumRange;
                                }
                                rgb_buffer_size += 4;
                            }

                            memset(dec_buffer_dcol, 0, dec_buffer_dcol_size);

                            //calc dyn percentage
                            //if part is before dyncol or is dyncol but before start frame, use start colors
                            //if part is after dyncol or is dyncol but after end frame, use end colors
                            //else calc
                            if (prefix.substr(0, descTxt.dyncol_part_name.length()) == descTxt.dyncol_part_name)
                            {
                                if (curr_frame > descTxt.dyncol_start && curr_frame <= descTxt.dyncol_end)
                                    dyn_percentage = (float)(curr_frame - descTxt.dyncol_start) / (descTxt.dyncol_end - descTxt.dyncol_start) * 100;
                            }

                            dyn_colors[0] = mixColors(descTxt.dyncol_start_colors[0], descTxt.dyncol_end_colors[0], dyn_percentage);
                            dyn_colors[1] = mixColors(descTxt.dyncol_start_colors[1], descTxt.dyncol_end_colors[1], dyn_percentage);
                            dyn_colors[2] = mixColors(descTxt.dyncol_start_colors[2], descTxt.dyncol_end_colors[2], dyn_percentage);
                            dyn_colors[3] = mixColors(descTxt.dyncol_start_colors[3], descTxt.dyncol_end_colors[3], dyn_percentage);

                            //apply dynamic coloring
                            ApplyDynamic(dec_buffer, dec_buffer_dcol, dyn_colors[0], dyn_colors[1], dyn_colors[2], dyn_colors[3],
                                image.columns(), image.rows());

                            if (hasTrim)
                            {
                                PasteImage(a, dec_buffer_dcol, pos[curr_frame].x, pos[curr_frame].y, pos[curr_frame].w, pos[curr_frame].h);
                            }
                            else
                            {
                                PasteImage(a, dec_buffer_dcol, 0, 0, image.columns(), image.rows());
                            }
                            delete[] dec_buffer_dcol;
                            delete[] dec_buffer;
                        }
                        else
                        {
                            dec_buffer = new uint8_t[image.columns() * image.rows() * 4];
                            dec_buffer_size = image.columns() * image.rows() * 4;
                            if (image.type() == PaletteType || image.type() == PaletteAlphaType
                                || image.type() == GrayscaleAlphaType || image.type() == TrueColorAlphaType) //for some reason palette images (e.g. gif files) have alpha channel, change rgb multiplier to 4
                                rgb_multi = 4;
                            else if (image.type() == GrayscaleType)
                                rgb_multi = 2;
                            //get data
                            const MagickCore::Quantum* pixelsArr = image.getConstPixels(0, 0, image.columns(), image.rows());
                            for (unsigned int i = 0; i < image.columns() * image.rows() * rgb_multi; i += rgb_multi) {
                                for (unsigned int i1 = 0; i1 < 2; i1++) {
                                    if (rgb_multi == 2)
                                    {
                                        dec_buffer[rgb_buffer_size] = pixelsArr[i] * 255 / QuantumRange;
                                        dec_buffer[rgb_buffer_size + 1] = pixelsArr[i] * 255 / QuantumRange;
                                        dec_buffer[rgb_buffer_size + 2] = pixelsArr[i] * 255 / QuantumRange;
                                    }
                                    else
                                    {
                                        dec_buffer[rgb_buffer_size] = pixelsArr[i + 0] * 255 / QuantumRange;
                                        dec_buffer[rgb_buffer_size + 1] = pixelsArr[i + 1] * 255 / QuantumRange;
                                        dec_buffer[rgb_buffer_size + 2] = pixelsArr[i + 2] * 255 / QuantumRange;
                                    }
                                }
                                rgb_buffer_size += 3;
                            }

                            if (hasTrim)
                            {
                                PasteImage(a, dec_buffer, pos[curr_frame].x, pos[curr_frame].y, pos[curr_frame].w, pos[curr_frame].h);
                            }
                            else
                            {
                                PasteImage(a, dec_buffer, 0, 0, image.columns(), image.rows());
                            }
                            delete[] dec_buffer;
                        }

                        if (video->AddFrame() == false)
                        {
                            printf("Failed to add frame to video\n");
                        }
                        else
                        {
                            printf("Frame added to video: %s\n", zipEntry.c_str());
                        }

                    }
                    catch (Exception& e) {
                        std::cerr << "Error processing image: " << e.what() << std::endl;
                    }
                }
                curr_frame++;
            }
            delete[] file_data;
        }
    }
}

void ParseAnimation()
{
    uint8_t* rgb_buffer = NULL;
    int rgb_buffer_size = 0;
    int16_t* audioData = audio_buffer;
    int samples = audio_samples;
    int samples_per_frame = 1024;
    uint32_t anim_total_ms = 0;
    uint32_t anim_curr_ms = 0;

    rgb_buffer = new uint8_t[descTxt.hdr.width * descTxt.hdr.height * 3];
    rgb_buffer_size = descTxt.hdr.width * descTxt.hdr.height * 3;

    Video* video = new Video();
    video->SetParams(descTxt.hdr.width, descTxt.hdr.height, descTxt.hdr.fps);
    if (audio_present) {
        video->SetParamsAudio(audio_sample_rate, samples_per_frame, audio_channels);
    }
    video->Open(outFileName);
    video->Start();
    video->CreateVideoTrack();
    if (audio_present) {
        video->CreateAudioTrack();
    }

    for (int entry = 0; entry < descTxt.entries.size(); entry++) {
        DescEntry& e = descTxt.entries[entry];
        std::string prefix = e.path + "/";
        for (const auto& zipEntry : zipEntries) {
            if (zipEntry.find(prefix) == 0 && zipEntry.length() > prefix.length()) {
                //check if ending in txt, skip if so
                if (zipEntry.substr(zipEntry.length() - 4) == ".txt") {
                    continue; // skip desc.txt files
                }
                descTxt.entries[entry].duration_ms += FpsToMs(descTxt.hdr.fps);
                anim_total_ms += FpsToMs(descTxt.hdr.fps) * (descTxt.entries[entry].count > 0 ? descTxt.entries[entry].count : 1);
            }
        }
    }

    if (audio_present) {
        audio->GetAudioDuration(&audio_total_ms);
    }

    printf("anim: %dms, audio: %dms\n", anim_total_ms, audio_total_ms);

    //parse entries
    for (int entry = 0; entry < descTxt.entries.size(); entry++) {
        DescEntry& e = descTxt.entries[entry];
        int x = 0, y = 0;
        int loop_times_parsed = 0;
        int total_loop_times = e.count;
        //parse an image
        //find every filename that starts with e.path
        std::string prefix = e.path + "/";

        if (total_loop_times > 0)
        {
            for (int l = 0; l < total_loop_times; l++)
            {
                ParseEntry(prefix, video, e.bkg_color, e.pause);
                anim_curr_ms += e.duration_ms;
            }
        }
        else
        {
            int max_dur = MAX(anim_total_ms, audio_total_ms);
            while (anim_curr_ms < max_dur)
            {
                ParseEntry(prefix, video, e.bkg_color, e.pause);
                anim_curr_ms += e.duration_ms;
            }
        }
    }
    video->AddFrame();
    //finish audio add
    if (audio_present) {
        while (samples > 0) {
            video->AddAudio(audioData, samples_per_frame);
            audioData += samples_per_frame * audio_channels;
            samples -= samples_per_frame;
        }
    }


    video->Close();
    delete[] rgb_buffer;
    rgb_buffer = NULL;
    rgb_buffer_size = 0;
}

int main(int argc, char* argv[])
{
	audio = new Audio();
    //init magick
    InitializeMagick(argv[0]);

    if (argc < 3) {
		printf("Usage: %s -anim <zipfile> [-audio <audiofile>] [-out <outputfile>] [-dynamic <color1> <color2> <color3> <color4>]\n", argv[0]);
        printf("-anim: Path to animation zip file\n");
        printf("-audio: Path to audio file played in animation.zip\n");
        printf("-dynamic: R G B and A mask colors to apply during and end of dynamic color change\n");
        printf("-out: Output file path(by default it would be <current cmd path>\bootanimation.mp4)\n");
        return 1;
	}

    //parse args...
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-anim") == 0 && i + 1 < argc) {
			char* zipFile = argv[++i];
            zipArchive = zip_open(zipFile, ZIP_RDONLY, nullptr);
        }
        else if (strcmp(argv[i], "-audio") == 0 && i + 1 < argc) {
            // Handle audio file
			char* audioFile = argv[++i];
            std::cout << "Audio file: " << audioFile << std::endl;
			audio->Open(std::string(audioFile));
			audio_present = true;
        }
        else if (strcmp(argv[i], "-out") == 0 && i + 1 < argc)
			outFileName = std::string(argv[++i]);
        else if (strcmp(argv[i], "-dynamic") == 0 && i + 1 < argc) {
            for (int m = 0; m < 4; m++)
            {
                char* color = argv[++i];
				int color_cvt = atoi(color);
                descTxt.dyncol_end_colors[m].R = ((color_cvt >> 16) & 0xFF);
                descTxt.dyncol_end_colors[m].G = ((color_cvt >> 8) & 0xFF);
                descTxt.dyncol_end_colors[m].B = (color_cvt & 0xFF);
            }
        }
	}

    if (audio_present)
    {
		audio->Convert();
        audio->ExtractInfo(&audio_sample_rate, &audio_samples, &audio_channels);
        audio_buffer = audio->GetAudioBuffer();
    }
    
    if (!zipArchive) {
        printf("No animation input\n");
        return 1;
    }
    ProcessZipFile();
    SortZipEntries();
    ExtractDescTxt();
    ParseAnimation();
    zip_close(zipArchive);
	return 0;
}
