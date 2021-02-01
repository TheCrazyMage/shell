#include <sys/wait.h>
#include <unistd.h>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include <queue>
using namespace std;

int checkSym(char ch)
{
	int key = (int)ch;
	if ((key >= (int)'a' && key <= (int)'z') || (key >= (int)'A' && key <= (int)'Z') || (ch == '_'))
		return 1;
	if (key >= (int)'0' && key <= (int)'9')
		return 2;
  if (ch =='#' || ch == '*' || ch == '@' || ch == '?' || ch == '!' || ch  == '$')
		return 3;
	return 0;
}

int checkVar(string line)
{
	if ( (line.size() == 1) && (checkSym(line[0])) )
		return 1;
		
	if ((line.size() > 1) && (checkSym(line[0]) == 1))
	{
		for (int i = 1; i < line.size(); ++i)
		{
			if (!(checkSym(line[i]) == 1 || checkSym(line[i]) == 2))
				return 0;
		}
		return 1;
	}
	else
		return 0;
		
	return 0;
}

int cd(vector<string> tokens)
{
  if (tokens[1].c_str() == nullptr) {
    fprintf(stderr, "shell: waiting for \"cd\" arguments\n");
  } 
  else 
  {
    if (chdir(tokens[1].c_str()) != 0) 
    {
      perror("shell");
    }
  }
  return 1;
}

int launch(vector<string> tokens)
{
	pid_t pid, wpid;
	int status;

  pid = fork();

  vector<char*> argsV;
  for(int i = 0; i < tokens.size(); ++i)
  {
    argsV.push_back(const_cast<char*>(tokens[i].c_str()));
  }
  argsV.push_back(nullptr);
  char** args = &argsV[0];

  if (pid == 0) 
  {
    if (execvp(args[0], args) == -1) 
    {
      perror("shell");
    }
    exit(EXIT_FAILURE);
  }
  else 
  {
    do 
    {
      wpid = waitpid(pid, &status, WUNTRACED);
    } while (!WIFEXITED(status) && !WIFSIGNALED(status));
  }

	return 1;
}

int vars(string line)
{
  char eq = '=';
	int eqNum = line.find('=');
	string varName = line.substr(0, eqNum);
  if (!checkVar(varName))
  {
    cerr <<  "shell: invalid variable name" << endl;
    return 1;
  }

  string varValue = line.substr(eqNum+1, line.size());

  if(setenv(varName.c_str(), varValue.c_str(), 1) == -1)
  {
    perror("shell: ");
    exit(0);
  }
  
  return 1;
}

int execute(vector<string> tokens)
{
  if (tokens.empty()) return 1;

  if (tokens[0].find('=') != string::npos)
    return vars(tokens[0]);

  if (tokens[0] == "cd")
    return cd(tokens);

  return launch(tokens);
}

class Parser
{
public:

	enum States { BEGIN = 1, STRING1 = 2, STRING2 = 3, COMMAND1 = 4, COMMAND2 = 5, VAR1 = 6, VAR2 = 7, TERMINATOR = 8 };
	Parser(string _line) : line(_line) { parse(); }
	vector<string> getTokens() { return tokens; }
	void printOrder()
	{
		for (int i = 0; i < order.size(); ++i)
		{
			cout << order[i].first << '\t' << order[i].second << endl;
		}
	}
	void printTokens()
	{
		for (int i = 0; i < tokens.size(); ++i)
		{
			cout << '[' << i << ']' << '\t';
			cout << tokens[i] << endl;
		}
	}

	void scan()
	{
		char startQuote; int pos = 0, brackets = 0, inside = 0;
		for (int i = 0; i < line.size(); ++i)
		{
			if (line[i] == '\\' && state == STRING1)
			{
				line.erase(i, 1);
				cout << "NOW1: [" << line << "]\t" << line.size() <<  endl;
				continue;
			}
			if (line[i] == '\\' && (!(line.size() - i - 2)))
			{
				--i;
				cout << "NOW2: [" << line << "]\t" << line.size() << endl;
			}
			switch (state)
			{
			case BEGIN:
				if (isspace(line[i]))
				{
					order.push_back(pair<States, unsigned>(TERMINATOR, i));
					break;
				}
				if (line[i] == '"' || line[i] == '\'')
				{
					startQuote = line[i];
					state = STRING1;
					order.push_back(pair<States, unsigned>(STRING1, i));
					break;
				}
				if (line[i] == '$')
				{
					state = VAR1;
					if (line[i + 1] == '{')
						order.push_back(pair<States, unsigned>(VAR1, i + 2));
					else
						order.push_back(pair<States, unsigned>(VAR1, i + 1));
					break;
				}
				state = COMMAND1;
				order.push_back(pair<States, unsigned>(COMMAND1, i));
				break;

			case STRING1:
				if ((line[i] == '"' && line[i] == startQuote) || (line[i] == '\'' && line[i] == startQuote))
				{
					order.push_back(pair<States, unsigned>(STRING2, i));
					if (inside)
						state = COMMAND1;
					else
						state = BEGIN;
				}
				break;

			case COMMAND1:
				if (isspace(line[i]))
				{
					order.push_back(pair<States, unsigned>(COMMAND2, i - 1));
					order.push_back(pair<States, unsigned>(TERMINATOR, i));
					state = BEGIN;
				}

				if (line[i] == '"' || line[i] == '\'')
				{
					order.push_back(pair<States, unsigned>(STRING1, i));
					state = STRING1;
					inside = 1;
					startQuote = line[i];
				}

				break;

			case VAR1:
				if (line[i] == '{')
					brackets = 1;

				if (line[i] == '}' && brackets)
				{
					order.push_back(pair<States, unsigned>(VAR2, i - 1));
					brackets = 0;
					state = BEGIN;
				}

				if ((isspace(line[i])) && !brackets)
				{
					order.push_back(pair<States, unsigned>(VAR2, i - 1));
					order.push_back(pair<States, unsigned>(TERMINATOR, i));
					state = BEGIN;
				}

				break;
			}
		}
	}

	void parse()
	{
		scan();

		int start = 0, end = 0, command = 0;
		for (int i = 0; i < order.size(); ++i)
		{
			switch (state = order[i].first)
			{
			case STRING1:
				if (command)
				{
					line[order[i].second] = 0;
					break;
				}
				start = order[i].second;
				line[start] = 0;
				break;

			case STRING2:
				end = order[i].second;
				line[end] = 0;
				if (!command)
					tokens.push_back(line.substr(start, end - start + 1));
				else
					line[order[i].second] = 0;
				break;

			case COMMAND1:
				command = 1;
				start = order[i].second;
				break;

			case COMMAND2:
				command = 0;
				end = order[i].second;
				tokens.push_back(line.substr(start, end - start + 1));
				break;

			case VAR1:
				start = order[i].second;
				break;

			case VAR2:
				end = order[i].second;
				if (checkVar(line.substr(start, end - start + 1)))
					tokens.push_back(line.substr(start, end - start+1));
				else
					cerr << "shell: invalid variable name" << endl;
				break;

			case TERMINATOR:
				start = 0;
				end = 0;
				break;
			}
		}
	}

private:
	string line;
	vector<string> tokens;
	vector<pair<States, unsigned>> order;
	States state = BEGIN;
};


vector<string> parseLine(string line)
{
	vector<string> tokens;
	Parser S(line);
	return S.getTokens();
}

void removeSpaces(std::string& line)
{
	while (isspace(line[0]))
	{
		line.erase(0, 1);
	}
	while (isspace(line[line.length() - 1]))
	{
		line.erase(line.length() - 1, 1);
	}
	line += ' ';
}

string readLine()
{
	string line;
	getline(std::cin, line);
	removeSpaces(line);
	if (line.empty())
		exit(EXIT_SUCCESS);
	return line;
}

void shell()
{
	string line;
	vector<string> args;
	bool status;

	do
	{
		cout << ">";
		line = readLine();
		args = parseLine(line);
		status = execute(args);
	} while (status);
}

int main()
{
	shell();
}
