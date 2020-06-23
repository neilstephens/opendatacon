/*
 * Modified for opendatacon to have a timeout in the blocking console I/O
 */

/*
 *
 * TinyCon - A tiny console library, written for C++
 * https://sourceforge.net/projects/tinycon/
 * ------------------------------------------------------
 * License:  BSD
 * Author:   Unix-Ninja | chris (at) unix-ninja (dot) com
 *
 * Terminal Console
 * Usage: tinyConsole tc("prompt>");
 *        tc.run();
 * 
 * When a command is entered, it will be passed to the trigger method for
 * processing. This is a blocking function.
 *
 * tinyConsole::trigger can be over-ridden in a derivative class to
 * change its functionality.
 *
 * Additionally, a check to the 'hotkeys' method is called for each
 * keypress, which can also be over-ridden in a dirivative class.
 *
 */

#include "tinycon.h"
#include <opendatacon/util.h>

#if !defined(WIN32) && !defined(_WIN32) && !defined(__WIN32)
#include <termios.h>
#include <thread>
#include <unistd.h>
#include <utility>
int GetCharTimeout (const uint8_t timeout_tenths_of_seconds)
{
	static const size_t secinaday = 10*60*60*24;
	static size_t err_backoff = timeout_tenths_of_seconds;
	static const auto err = [&](const std::string& msg)
	{
		if(auto log = odc::spdlog_get("ConsoleUI"))
			log->error("{}. Sleeping for {} ms", msg, 100*err_backoff);
		std::this_thread::sleep_for(std::chrono::milliseconds(100*err_backoff));

		if(err_backoff < secinaday)
			err_backoff *= 2;
		else if(err_backoff > secinaday)
			err_backoff = secinaday;
	};

	struct termios oldt{}, newt{};
	int ch = 0;

	if(tcgetattr(STDIN_FILENO, &oldt) != 0)
	{
		err("Failed to get terminal attributes");
		return 0;
	}

	newt = oldt;
	newt.c_lflag &= ~(ICANON | ECHO);
	newt.c_cc[VTIME] = timeout_tenths_of_seconds;
	newt.c_cc[VMIN] = 0;

	if(tcsetattr(STDIN_FILENO, TCSANOW, &newt) != 0)
	{
		err("Failed to set terminal attributes");
		return 0;
	}

	err_backoff = timeout_tenths_of_seconds;
	auto rv = read(STDIN_FILENO,&ch,1);
	if(rv == 1)
	{
		if(ch == ESC)
		{
			if(read(STDIN_FILENO,&ch,1) == 1)
			{
				if(ch == 91)
				{
					ch = KEY_CTRL1;
				}
			}
		}
	}
	else if(rv != 0)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(100*timeout_tenths_of_seconds));
	}

	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
	return ch;
}
#else
//Windows
#include <conio.h>
#include <Windows.h>
int GetCharTimeout(const uint8_t timeout_tenths_of_seconds)
{
	while (1)
	{
		switch (WaitForSingleObject(GetStdHandle(STD_INPUT_HANDLE), timeout_tenths_of_seconds * 100))
		{
		case(WAIT_TIMEOUT):
			return 0;
		case(WAIT_OBJECT_0):
			if (_kbhit()) // _kbhit() always returns immediately
			{
				return _getch();
			}
			else // some sort of other events , we need to clear it from the queue
			{
				// clear event
				INPUT_RECORD record;
				DWORD numRead;
				ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &record, 1, &numRead);
				break;
			}
		case(WAIT_FAILED):
		case(WAIT_ABANDONED):
		default:
			return 0;
		}
	}
}
#endif

tinyConsole::tinyConsole ()
{
	_max_history = MAX_HISTORY;
	_quit = false;
	pos = -1;
	line_pos = 0;
	skip_out = 0;
}
tinyConsole::~tinyConsole ()
{}

tinyConsole::tinyConsole (std::string s)
{
	_max_history = MAX_HISTORY;
	_quit = false;
	_prompt = std::move(s);
	pos = -1;
	line_pos = 0;
	skip_out = 0;
}

std::string tinyConsole::version ()
{
	return TINYCON_VERSION;
}

void tinyConsole::pause ()
{
	while(GetCharTimeout(5) == 0);
}

void tinyConsole::quit ()
{
	_quit = true;
}

int tinyConsole::trigger (const std::string& cmd)
{
	if (cmd == "exit") {
		_quit = true;
	} else {
		std::cout << cmd << std::endl;
	}
	return 0;
}

int tinyConsole::hotkeys (char c)
{
	return 0;
}

void tinyConsole::setMaxHistory (int max)
{
	_max_history = max;
}

std::string tinyConsole::getLine ()
{
	return getLine(M_LINE, "");
}

std::string tinyConsole::getLine (int mode)
{
	return getLine(mode, "");
}

std::string tinyConsole::getLine (int mode = M_LINE, const std::string& delimeter = "")
{
	std::string line;
	char c;

	for (;;)
	{
		while((c = GetCharTimeout(5)) == 0);
		if ( c == NEWLINE)
		{
			std::cout << std::endl;
			break;
		} else if ((int) c == BACKSPACE ) {
			if (line.length())
			{
				line = line.substr(0,line.size()-1);
				if (mode != M_PASSWORD)
				{
					std::cout << "\b \b" << std::flush;
				}
			}
		} else {
			line += c;
			if (mode != M_PASSWORD)
			{
				std::cout << c << std::flush;
			}
		}
	}
	return line;
}

void tinyConsole::setBuffer (std::string s)
{
	buffer.assign(s.begin(),s.end());
	line_pos = buffer.size();
}

void tinyConsole::run ()
{
	//show prompt
	std::cout << _prompt << std::flush;

	// grab input
	for (;;)
	{
		if(_quit)break;
		c = GetCharTimeout(5);
		if(c == 0)continue;
		if(!hotkeys(c))
		switch (c)
		{
			case ESC:
				//FIXME: escape is only detected if double-pressed.
				std::cout << "(Esc)"<< std::flush;
				break;
			case KEY_CTRL1: // look for arrow keys
				switch (c = GetCharTimeout(5))
				{
					case UP_ARROW:
						if (!history.size()) break;
						if (pos == -1)
						{
							// store current command
							unused = "";
							unused.assign(buffer.begin(), buffer.end());
						}

						// clear line
						for (int i = 0; i < line_pos; i++)
						{
							std::cout << "\b \b" << std::flush;
						}

						// clean buffer
						buffer.erase(buffer.begin(), buffer.end());

						pos++;
						if (pos > ((int)history.size() - 1)) pos = history.size() - 1;

						// store in buffer
						for (size_t i = 0; i < history[pos].size(); i++)
						{
							buffer.push_back(history[pos][i]);
						}
						line_pos = buffer.size();
						// output to screen
						std::cout << history[pos] << std::flush;
						break;
					case DOWN_ARROW:
						if (!history.size()) break;

						// clear line
						for (int i = 0; i < line_pos; i++)
						{
							std::cout << "\b \b" << std::flush;
						}

						// clean buffer
						buffer.erase(buffer.begin(), buffer.end());

						pos--;
						if (pos<-1) pos = -1;
						if (pos >= 0) {
							std::cout << history[pos] << std::flush;
							// store in buffer
							for (size_t i = 0; i < history[pos].size(); i++)
							{
								buffer.push_back(history[pos][i]);
							}
						} else {
							if (buffer.size())
							{
								std::cout << unused << std::flush;
								// store in buffer
								for (size_t i = 0; i < unused.size(); i++)
								{
									buffer.push_back(unused[i]);
								}
							}
						}
						line_pos = buffer.size();
						break;
					case LEFT_ARROW:
						// if there are characters to move left over, do so
						if (line_pos)
						{
							std::cout << "\b" << std::flush;
							line_pos--;
						}
						break;
					case RIGHT_ARROW:
						// if there are characters to move right over, do so
						if (line_pos < (int)buffer.size())
						{
							std::cout << buffer[line_pos] << std::flush;
							line_pos++;
						}
						break;
					case DEL:
						if (line_pos < (int)buffer.size())
						{
							skip_out = 1;
							buffer.erase(buffer.begin()+line_pos);
							// update screen after current position
							for (int i = line_pos; i < (int)buffer.size(); i++ )
							{
								std::cout << buffer[i] << std::flush;
							}
							// erase last char
							std::cout << " ";
							for (int i = line_pos; i < (int)buffer.size(); i++ )
							{
								std::cout << "\b" << std::flush;
							}
							// make-up for erase position
							std::cout << "\b" << std::flush;
							//std::cout << "(DEL)" << std::flush;
						}
						break;
					default:
						skip_out = 1;
						//std::cout << "(" << (int) c << ")" << std::endl;
					}
				break;
			case BACKSPACE:
				if (line_pos == 0) break;
				// move cursor back, blank char, and move cursor back again
				std::cout << "\b \b"<< std::flush;
				// don't forget to clean the buffer and update line position
				if (line_pos == (int)buffer.size()) {
					buffer.pop_back();
					line_pos--;
				} else {
					line_pos--;
					buffer.erase(buffer.begin()+line_pos);
					// update screen after current position
					for (int i = line_pos; i < (int)buffer.size(); i++ ) {
						std::cout << buffer[i]<< std::flush;
					}
					// erase last char
					std::cout << " "<< std::flush;
					for (int i = line_pos+1; i < (int)buffer.size(); i++ ) {
						std::cout << "\b"<< std::flush;
					}
					// make-up for erase position and go to new position
					std::cout << "\b\b"<< std::flush;
				}
				break;
			case TAB:
				break;
				// print history
				for (size_t i = 0; i < history.size(); i++) {
					std::cout << history[i] << std::endl;
				}
				break;
			case NEWLINE:
				// store in string
				s.assign(buffer.begin(), buffer.end());

				
				
				// save command to history
				// trimming of command should be done in callback function
				if(s.length()) 
					history.push_front(s);

				// run command
				//(*callbackFunc)(s.c_str());
				std::cout << std::endl;
				trigger(s);
				
				// check for exit command
				if(_quit == true) {
					return;
				}
				
				if (history.size() > _max_history) history.pop_back();

				// clean buffer
				buffer.erase(buffer.begin(), buffer.end());

				// print prompt. new line should be added from callback function
				std::cout << _prompt << std::flush;

				// reset position
				pos = -1;
				line_pos = 0;
				break;
			default:
				if (skip_out) {
					skip_out = 0;
					break;
				}
				std::cout << c << std::flush;
				if(line_pos == (int)buffer.size()) {
					// line position is at the end of the buffer, just append
					buffer.push_back(c);
				} else {
					// line position is not at end. Insert new char
					buffer.insert(buffer.begin()+line_pos, c);
					// update screen after current position
					for (int i = (int)line_pos+1; i < (int)buffer.size(); i++ ) {
						std::cout << buffer[i]<< std::flush;
					}
					for (int i = (int)line_pos+1; i < (int)buffer.size(); i++ ) {
						std::cout << "\b"<< std::flush;
					}
				}
				line_pos++;
				break;
		} // end switch
	}

}
