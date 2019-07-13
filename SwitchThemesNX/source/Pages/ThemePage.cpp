#include "ThemePage.hpp"
#include "../ViewFunctions.hpp"
#include <algorithm>
#include "../Platform/Platform.hpp"

#include "../UI/imgui/imgui_internal.h"

using namespace std;

static constexpr int LimitLoad = 25;

ThemesPage::ThemesPage(const std::vector<std::string> &files) : 
lblPage(""),
NoThemesLbl("There's nothing here, copy your themes in the themes folder on your sd and try again")
{
	Name = "Themes";
	ThemeFiles = files;
	lblCommands = CommandsTextNormal;
	std::sort(ThemeFiles.begin(), ThemeFiles.end());
	SetDir(SD_PREFIX "/themes");
}

ThemesPage::~ThemesPage()
{
	SetPage(-1);
}

void ThemesPage::SetDir(const string &dir)
{
	CurrentDir = dir;
	if (!StrEndsWith(dir, "/"))
		CurrentDir += "/";
	
	CurrentFiles.clear();
	for (auto f : ThemeFiles)
	{
		if (GetPath(f) == CurrentDir)
			CurrentFiles.push_back(f);
	}
	
	pageCount = CurrentFiles.size() / LimitLoad + 1;
	if (CurrentFiles.size() % LimitLoad == 0)
		pageCount--;
	menuIndex = 0;
	SetPage(0);
}

void ThemesPage::SetPage(int num)
{
	ResetScroll = true;
	ImGui::NavMoveRequestCancel();
	if (pageNum != num)
		menuIndex = 0;
	for (auto i : DisplayEntries)
		delete i;
	DisplayEntries.clear();
	
	int baseIndex = num * LimitLoad;
	if (num < 0 || baseIndex >= CurrentFiles.size())  
	{
		lblPage = (CurrentDir + " - Empty");
		pageNum = num;
		return;
	}
	
	DisplayLoading("Loading...");
	int imax = CurrentFiles.size() - baseIndex;
	if (imax > LimitLoad) imax = LimitLoad;
	for (int i = 0; i < imax; i++)
	{
		auto entry = new ThemeEntry(CurrentFiles[baseIndex + i]);
		/*if (IsSelected(CurrentFiles[baseIndex + i]))
			entry->Highlighted = true;*/
		DisplayEntries.push_back(entry);
	}
	pageNum = num;
	auto LblPStr = CurrentDir + " - Page " + to_string(num + 1) + "/" + to_string(pageCount);
	if (SelectedFiles.size() != 0)
		LblPStr = "("+ to_string(SelectedFiles.size()) + " selected) " + LblPStr;
	lblPage = (LblPStr);
}

const int EntryW = 860;
void ThemesPage::Render(int X, int Y)
{
	Utils::ImGuiSetupPage("ThemesPageContainer", X, Y, DefaultWinFlags | ImGuiWindowFlags_NoBringToFrontOnFocus);
	ImGui::PushFont(font25);

	if (DisplayEntries.size() == 0)
	{
		ImGui::Text(NoThemesLbl.c_str());
		goto QUIT_RENDERING;
	}

	ImGui::SetCursorPosY(570);
	ImGui::Text(lblCommands.c_str());

	ImGui::SetCursorPosY(600);
	Utils::ImGuiRightString(lblPage);

	{
		Utils::ImGuiSetupPage(this, X, Y, DefaultWinFlags & ~ImGuiWindowFlags_NoScrollbar);
		int setNewMenuIndex = 0;
		if (ResetScroll)
		{
			setNewMenuIndex = menuIndex;
			ImGui::SetScrollY(0);
			ImGui::SetNavID(0, 0);
			ResetScroll = false;
		}
		ImGui::SetWindowSize(TabPageArea);
		{
			int count = 0;
			for (auto& e : DisplayEntries)
			{
				bool Selected = IsSelected(e->GetPath());
				if (Selected)
					ImGui::PushStyleColor(ImGuiCol_Button, 0x366e64ff);

				if (e->IsHighlighted())
					menuIndex = count;
				auto res = e->Render(Selected);

				if (Selected)
					ImGui::PopStyleColor();
				if (count == setNewMenuIndex && FocusEvent.Reset()) Utils::ImGuiSelectItem(true);
				if (ImGui::IsItemActive())
				{
					ImVec2 drag = ImGui::GetMouseDragDelta(0);
					ImGui::SetScrollY(ImGui::GetScrollY() - drag.y);
				}

				if (res == ThemeEntry::UserAction::Preview)
					break;
				else if (res == ThemeEntry::UserAction::Install)
					PushFunction([count, &e, this]()
						{
							if (e->IsFolder)
								SetDir(e->GetPath());
							else
							{
								if (SelectedFiles.size() == 0)
								{
									if (gamepad.buttons[GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER])
									{
										DisplayLoading("Installing to shuffle...");
										e->InstallTheme(false, MakeThemeShuffleDir());
									}
									else
										e->InstallTheme();
								}
								else
								{
									if (menuIndex != count)
										menuIndex = count;
									SelectCurrent();
								}
							}
						});

				count++;
			}
		}

	QUIT_RENDERING_FROMCHILD:

		Utils::ImGuiSetWindowScrollable();
		Utils::ImGuiCloseWin();
	}
QUIT_RENDERING:
	ImGui::PopFont();
	Utils::ImGuiCloseWin();
}

int ThemesPage::PageItemsCount()
{
	int menuCount = CurrentFiles.size() - pageNum * LimitLoad;
	if (menuCount > LimitLoad)
		menuCount = LimitLoad;
	return menuCount;
}

inline bool ThemesPage::IsSelected(const std::string &fname)
{
	return (std::find(SelectedFiles.begin(), SelectedFiles.end(), fname) != SelectedFiles.end());
}

void ThemesPage::SelectCurrent()
{
	if (DisplayEntries[menuIndex]->IsFolder) return;
	auto fname = DisplayEntries[menuIndex]->GetPath();
	auto position = std::find(SelectedFiles.begin(), SelectedFiles.end(), fname);
	if (position != SelectedFiles.end())
	{
		SelectedFiles.erase(position);
	}
	else 
	{
		SelectedFiles.push_back(fname);
	}
	lblCommands = (SelectedFiles.size() == 0 ? CommandsTextNormal : CommandsTextSelected);
}

void ThemesPage::Update()
{
	if (DisplayEntries.size() == 0)
		Parent->PageLeaveFocus(this);

	int menuCount = PageItemsCount();	
	
	if (KeyPressed(GLFW_GAMEPAD_BUTTON_DPAD_LEFT))
		Parent->PageLeaveFocus(this);
	if (KeyPressed(GLFW_GAMEPAD_BUTTON_B))
	{
		if (CurrentDir != SD_PREFIX "/themes/")
			SetDir(GetParentDir(CurrentDir));
		else 
			Parent->PageLeaveFocus(this);
	}
	
	if (menuCount <= 0)
		return;
	

	if (KeyPressed(GLFW_GAMEPAD_BUTTON_DPAD_UP) && menuIndex <= 0)
	{
		if (pageNum > 0)
		{
			SetPage(pageNum - 1);
			menuIndex = PageItemsCount() - 1;
			return;
		}
		else if (pageCount > 1)
		{
			SetPage(pageCount - 1);
			menuIndex = PageItemsCount() - 1;
			return;
		}
		else menuIndex = PageItemsCount() - 1;
	}
	else if (KeyPressed(GLFW_GAMEPAD_BUTTON_DPAD_DOWN) && menuIndex >= PageItemsCount() - 1)
	{
		if (pageCount > pageNum + 1) {
			SetPage(pageNum + 1);
			return;
		}
		else if (pageNum != 0)
		{
			SetPage(0);
			return;
		}
		else menuIndex = 0;
	}
	else if ((KeyPressed(GLFW_GAMEPAD_BUTTON_Y)) && menuIndex >= 0)
	{
		SelectCurrent();
	}
	else if (KeyPressed(GLFW_GAMEPAD_BUTTON_X))
	{
		SelectedFiles.clear();
	}
	else if (KeyPressed(GLFW_GAMEPAD_BUTTON_START) && SelectedFiles.size() != 0)
	{
		string shuffleDir = "";
		if (gamepad.buttons[GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER])
			shuffleDir = MakeThemeShuffleDir();
		for (string file : SelectedFiles)
		{
			DisplayLoading("Installing " + file + "...");
			ThemeEntry t {file};
			if (!t.InstallTheme(false,shuffleDir))
			{
				Dialog("Installing a theme failed, the process was cancelled");
				break;
			}
		}
		SelectedFiles.clear();
		SetPage(pageNum);		
	}
}