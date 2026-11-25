#include "global.h"
#include "LightsDriver_DDRMini.h"

#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "GameInput.h"
#include "LightsManager.h"
#include "RageLog.h"

REGISTER_LIGHTS_DRIVER( DDRMini );

namespace
{
const char *LED_PATHS[] = {
	"/sys/class/leds/button1/brightness",
	"/sys/class/leds/button2/brightness",
	"/sys/class/leds/button3/brightness",
	"/sys/class/leds/button4/brightness",
	"/sys/class/leds/button5/brightness",
	"/sys/class/leds/button6/brightness",
	"/sys/class/leds/yellow1/brightness",
	"/sys/class/leds/yellow2/brightness",
	"/sys/class/leds/red1/brightness",
	"/sys/class/leds/red2/brightness",
	"/sys/class/leds/speaker_l/brightness",
	"/sys/class/leds/speaker_r/brightness"
};

const char *FALLBACK_PAD_PATHS[] = {
	"/dev/input/hidraw_p1",
	"/dev/input/hidraw_p2"
};

struct PhysicalDevice
{
	std::string topology;
	std::string device;
};

std::string ResolvePath( const std::string &path )
{
	char resolved[PATH_MAX];
	if( realpath(path.c_str(), resolved) == NULL )
		return std::string();
	return resolved;
}

/* Reduce interface-specific input and hidraw paths to the physical USB
 * device path, allowing the two interfaces of one pad to be correlated. */
std::string USBTopology( const std::string &resolved )
{
	size_t componentStart = 0;
	while( componentStart < resolved.size() )
	{
		const size_t componentEnd = resolved.find( '/', componentStart+1 );
		const size_t end = componentEnd == std::string::npos ? resolved.size() : componentEnd;
		const std::string component = resolved.substr( componentStart+1, end-componentStart-1 );
		const size_t colon = component.find( ':' );
		if( !component.empty() && std::isdigit((unsigned char)component[0]) &&
			component.find('-') != std::string::npos && colon != std::string::npos )
		{
			return resolved.substr(0, componentStart+1) + component.substr(0, colon);
		}
		if( componentEnd == std::string::npos )
			break;
		componentStart = componentEnd;
	}
	return resolved;
}

std::vector<PhysicalDevice> EnumerateClass( const char *directoryName,
	const char *prefix, const char *devicePrefix, bool padsOnly )
{
	std::vector<PhysicalDevice> result;
	DIR *directory = opendir( directoryName );
	if( directory == NULL )
		return result;

	const size_t prefixLength = std::strlen( prefix );
	struct dirent *entry;
	while( (entry = readdir(directory)) != NULL )
	{
		if( std::strncmp(entry->d_name, prefix, prefixLength) != 0 )
			continue;
		const std::string classPath = std::string(directoryName) + "/" + entry->d_name;
		if( padsOnly )
		{
			std::string vendor, product;
			std::ifstream(classPath + "/device/id/vendor") >> vendor;
			std::ifstream(classPath + "/device/id/product") >> product;
			if( vendor != "33dd" || product != "0014" )
				continue;
		}
		const std::string resolved = ResolvePath( classPath );
		if( resolved.empty() )
			continue;
		PhysicalDevice found;
		found.topology = USBTopology( resolved );
		found.device = std::string(devicePrefix) + entry->d_name;
		result.push_back( found );
	}
	closedir( directory );
	std::sort( result.begin(), result.end(), [](const PhysicalDevice &a, const PhysicalDevice &b) {
		return a.topology == b.topology ? a.device < b.device : a.topology < b.topology;
	} );
	return result;
}

std::array<std::string, 2> DiscoverPadLights()
{
	std::vector<PhysicalDevice> inputs = EnumerateClass(
		"/sys/class/input", "js", "/dev/input/", true );
	if( inputs.empty() )
		inputs = EnumerateClass( "/sys/class/input", "event", "/dev/input/", true );
	const std::vector<PhysicalDevice> hidraw = EnumerateClass(
		"/sys/class/hidraw", "hidraw", "/dev/", false );

	std::multimap<std::string, std::string> outputs;
	for( unsigned i=0; i<hidraw.size(); ++i )
		outputs.insert( std::make_pair(hidraw[i].topology, hidraw[i].device) );

	std::array<std::string, 2> result;
	unsigned player = 0;
	for( unsigned i=0; i<inputs.size() && player<result.size(); ++i )
	{
		std::multimap<std::string, std::string>::iterator match = outputs.find(inputs[i].topology);
		if( match == outputs.end() )
		{
			LOG->Warn( "DDR Mini lights: no hidraw output matched %s at %s",
				inputs[i].device.c_str(), inputs[i].topology.c_str() );
			continue;
		}
		result[player] = match->second;
		LOG->Info( "DDR Mini lights: P%u %s -> %s (%s)", player+1,
			inputs[i].device.c_str(), result[player].c_str(), inputs[i].topology.c_str() );
		outputs.erase( match );
		++player;
	}

	if( player == 0 )
	{
		for( unsigned i=0; i<result.size(); ++i )
		{
			result[i] = FALLBACK_PAD_PATHS[i];
			LOG->Warn( "DDR Mini lights: P%u falling back to %s", i+1, result[i].c_str() );
		}
	}
	else if( player < result.size() )
		LOG->Warn( "DDR Mini lights: only %u correlated pad output(s) found", player );
	return result;
}

uint8_t Brightness( bool enabled ) { return enabled ? 255 : 0; }

bool WriteAll( int fd, const void *buffer, size_t length )
{
	const uint8_t *bytes = static_cast<const uint8_t *>(buffer);
	while( length > 0 )
	{
		const ssize_t written = write( fd, bytes, length );
		if( written < 0 && errno == EINTR )
			continue;
		if( written <= 0 )
			return false;
		bytes += written;
		length -= written;
	}
	return true;
}
}

LightsDriver_DDRMini::LightsDriver_DDRMini(): m_bFirstUpdate(true)
{
	m_LEDFDs.fill( -1 );
	m_PadFDs.fill( -1 );
	m_PreviousLEDs.fill( 0 );
	for( unsigned i=0; i<m_PreviousPads.size(); ++i )
		m_PreviousPads[i].fill( 0 );

	for( unsigned i=0; i<NUM_LEDS; ++i )
	{
		m_LEDFDs[i] = open( LED_PATHS[i], O_WRONLY|O_CLOEXEC );
		if( m_LEDFDs[i] < 0 )
			LOG->Warn( "DDR Mini lights: failed to open %s: %s", LED_PATHS[i], strerror(errno) );
	}

	const std::array<std::string, NUM_PADS> paths = DiscoverPadLights();
	for( unsigned i=0; i<NUM_PADS; ++i )
	{
		if( paths[i].empty() )
			continue;
		m_PadFDs[i] = open( paths[i].c_str(), O_WRONLY|O_CLOEXEC );
		if( m_PadFDs[i] < 0 )
			LOG->Warn( "DDR Mini lights: failed to open P%u output %s: %s",
				i+1, paths[i].c_str(), strerror(errno) );
	}
}

LightsDriver_DDRMini::~LightsDriver_DDRMini()
{
	for( unsigned i=0; i<NUM_LEDS; ++i )
	{
		if( m_LEDFDs[i] < 0 )
			continue;
		lseek( m_LEDFDs[i], 0, SEEK_SET );
		WriteAll( m_LEDFDs[i], "0", 1 );
		close( m_LEDFDs[i] );
	}
	const uint8_t off[PAD_REPORT_SIZE] = { 0x05, 0, 0, 0, 0 };
	for( unsigned i=0; i<NUM_PADS; ++i )
	{
		if( m_PadFDs[i] < 0 )
			continue;
		WriteAll( m_PadFDs[i], off, sizeof(off) );
		close( m_PadFDs[i] );
	}
}

void LightsDriver_DDRMini::WriteLED( unsigned index, uint8_t value, bool force )
{
	if( !force && m_PreviousLEDs[index] == value )
		return;
	const int fd = m_LEDFDs[index];
	if( fd < 0 || lseek(fd, 0, SEEK_SET) < 0 )
		return;
	const char *text = value ? "255" : "0";
	const size_t length = value ? 3 : 1;
	if( WriteAll(fd, text, length) )
		m_PreviousLEDs[index] = value;
}

void LightsDriver_DDRMini::WritePad( unsigned index,
	const std::array<uint8_t, PAD_REPORT_SIZE> &report, bool force )
{
	if( !force && m_PreviousPads[index] == report )
		return;
	if( m_PadFDs[index] >= 0 && WriteAll(m_PadFDs[index], report.data(), report.size()) )
		m_PreviousPads[index] = report;
}

void LightsDriver_DDRMini::Set( const LightsState *ls )
{
	if( ls == NULL )
		return;
	/* Gameplay drives these as game-button lights, while ScreenTestLights
	 * drives the cabinet ButtonsLeft/ButtonsRight channels.  They represent
	 * the same physical lamps on the DDR Mini. */
	const uint8_t p1Start = Brightness(
		ls->m_bGameButtonLights[GAME_CONTROLLER_1][GAME_BUTTON_START] ||
		ls->m_bCabinetLights[LIGHT_BUTTONS_LEFT] );
	const uint8_t p2Start = Brightness(
		ls->m_bGameButtonLights[GAME_CONTROLLER_2][GAME_BUTTON_START] ||
		ls->m_bCabinetLights[LIGHT_BUTTONS_RIGHT] );
	const uint8_t speaker = Brightness(ls->m_bCabinetLights[LIGHT_BASS_LEFT] ||
		ls->m_bCabinetLights[LIGHT_BASS_RIGHT]);

	const uint8_t leds[NUM_LEDS] = {
		p1Start, p1Start, p1Start,
		p2Start, p2Start, p2Start,
		Brightness(ls->m_bCabinetLights[LIGHT_MARQUEE_UP_LEFT]),
		Brightness(ls->m_bCabinetLights[LIGHT_MARQUEE_LR_LEFT]),
		Brightness(ls->m_bCabinetLights[LIGHT_MARQUEE_LR_RIGHT]),
		Brightness(ls->m_bCabinetLights[LIGHT_MARQUEE_UP_RIGHT]),
		speaker, speaker
	};
	for( unsigned i=0; i<NUM_LEDS; ++i )
		WriteLED( i, leds[i], m_bFirstUpdate );

	std::array<uint8_t, PAD_REPORT_SIZE> pads[NUM_PADS];
	pads[0] = {{ 0x05,
		Brightness(ls->m_bGameButtonLights[GAME_CONTROLLER_1][DANCE_BUTTON_RIGHT]),
		Brightness(ls->m_bGameButtonLights[GAME_CONTROLLER_1][DANCE_BUTTON_DOWN]),
		Brightness(ls->m_bGameButtonLights[GAME_CONTROLLER_1][DANCE_BUTTON_LEFT]),
		Brightness(ls->m_bGameButtonLights[GAME_CONTROLLER_1][DANCE_BUTTON_UP]) }};
	pads[1] = {{ 0x05,
		Brightness(ls->m_bGameButtonLights[GAME_CONTROLLER_2][DANCE_BUTTON_RIGHT]),
		Brightness(ls->m_bGameButtonLights[GAME_CONTROLLER_2][DANCE_BUTTON_DOWN]),
		Brightness(ls->m_bGameButtonLights[GAME_CONTROLLER_2][DANCE_BUTTON_LEFT]),
		Brightness(ls->m_bGameButtonLights[GAME_CONTROLLER_2][DANCE_BUTTON_UP]) }};
	for( unsigned i=0; i<NUM_PADS; ++i )
		WritePad( i, pads[i], m_bFirstUpdate );
	m_bFirstUpdate = false;
}
