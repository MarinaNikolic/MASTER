#!/usr/bin/python
#################################################################################
#   Program for generating and viewing gcov files                               #
#   Workspace is choosen from menu bar                                          #
#   click on the button to genrate gcov files for selected workspace            #
#   Process name is choosen from text field                                     #
#   click on the button to dump coverage data for selected process              #
#   Click on the name of a particular file from the tree to see it's content    #
#################################################################################

#Graficall support
import wx
#Directory acces support
import os.path, dircache
#Regular expressions support
import re
#Scrolling pannel support
import wx.lib.scrolledpanel

#support for reading .gcno files
#Choosing workspace via workspace_choosing_button won't work otherwise
import sys
reload(sys)
sys.setdefaultencoding("ISO-8859-1")

#Popup window for displaying help 
class HelpPopup(wx.PopupWindow):
	def __init__(self, parent, style):
		#Calling the constructor of the parent class
     	  	super(HelpPopup, self).__init__(parent, style)
		#defining a panel on popup window
        	self.help_panel = wx.Panel(self)
		#definig a static text display on panel for showing help content
		self.help_text = wx.StaticText(self.help_panel, -1,
                           "Workflow:\n\n"
                           "1. Choose workspace:\n"
                           "    - press button: \"Choose workspace\"\n"
                           "    - sellect root directory of your project\n\n"
                           "(unix exclusive)\n"
                           "2. Enter process name:\n"
                           "    - make sure that you have started the program you have instrumented\n"
                           "    - only if process is running, it can dump data\n"
                           "    - enter the name of your program/process into the text field that is top left.\n\n"
                           "(unix exclusive)\n"
                           "3. Dump coverage data:\n"
                           "    - press button: \"Dump coverage data\"\n"
                           "    - this will send signal to the process entered in step 2 to dump coverage data\n\n"
                           "4. Generate reports:\n"
                           "    - press button: \"Generate gcov and function files\"\n"
                           "    - this will generate two reports for every source file:\n"
                           "        -- report with extension gcov, that contains coverage information per line\n"
                           "        -- report with extension fun, that contains coverage information per function\n\n"
                           "5. Generate total coverage:\n"
                           "    - press button: \"Generate total project coverage\"\n"
                           "    - this will calculate coverage percentage of entire project\n\n\n"

                           "Notes:\n\n"
                           "1. Follow steps in exact order, in exception of:\n"
                           "    - if you are not using unix, please gather coverage data (gcda files) from your program and skip steps two and three\n"
                           "    - if you have already gathered coverage data (gcda files), please skip steps two and three\n"
                           "    - if you have already gcov reports in your workspace, you can do only steps one and five\n\n"
                           "2. In case of an issue, please send mail with detailed description on:\n"
                           "       Marina.Nikolic@rt-rk.com\n\n\n"
                           "Right click help window to close it"
                           ,
                           pos=(10,10))
		
		# retrieving size of help text to shape containing components
		text_size = self.help_text.GetBestSize()
		#resizing popup window and panel to fit text size. This is important so all text is visible
		self.SetSize((text_size.width+10, text_size.height+10))
		self.help_panel.SetSize((text_size.width+10, text_size.height+10))
		
		#binding event to pressing left mouse button that will calculate and remember location of binded component and start mouse event
		self.help_panel.Bind(wx.EVT_LEFT_DOWN, self.start_moving_event)
		self.help_text.Bind(wx.EVT_LEFT_DOWN, self.start_moving_event)

		#binding event to motion on unreleased click of left mouse button that will move the binded component
		self.help_panel.Bind(wx.EVT_MOTION, self.move_selected)
		self.help_text.Bind(wx.EVT_MOTION, self.move_selected)

		#binding event releasing left mouse button that will end mouse event
		self.help_panel.Bind(wx.EVT_LEFT_UP, self.end_moving_event)
		self.help_text.Bind(wx.EVT_LEFT_UP, self.end_moving_event)

		#binding event releasing right mouse button that will close the help popup
		self.help_panel.Bind(wx.EVT_RIGHT_UP, self.close_popup)
		self.help_text.Bind(wx.EVT_RIGHT_UP, self.close_popup)
		
		#setting help popup window in the centre of the screen
		self.Centre()   
	
	#Function for calculating original possition of component and capturing beggining of mouse event
	#It is called when left mouse button is pressed (before releasing)
	#Arguments: component name and binding event (generic: self, event)
	#No return value
	def start_moving_event(self, evt):
		self.Refresh()
		# Obtaining possition of the mouse relative to popup and converting to screen coordinates
		self.old_mouse_position = evt.GetEventObject().ClientToScreen(evt.GetPosition())
		# Calculating popup window's screen coordinates
		self.window_position = self.ClientToScreen((0,0))
		#Staring mouse event
		self.help_panel.CaptureMouse()

	#Function for calculating new possition of component and moving it accordinly
	#It is called when mouse is being moved, while left mouse button is pressed 
	#Arguments: component name and binding event (generic: self, event)
	#No return value
	def move_selected(self, evt):
		if evt.Dragging() and evt.LeftIsDown():
			# Obtaining possition of the mouse relative to popup and converting to screen coordinates
			new_mouse_position = evt.GetEventObject().ClientToScreen(evt.GetPosition())
			# calculating new position where th moving shoudl be done to
			new_position = (self.window_position.x + (new_mouse_position.x - self.old_mouse_position.x), self.window_position.y + (new_mouse_position.y - self.old_mouse_position.y))
			#moving
			self.Move(new_position)

	#Function for capturing end of mouse event
	#It is called when left mouse button is released
	#Arguments: component name and binding event (generic: self, event)
	#No return value
	def end_moving_event(self, evt):
		if self.help_panel.HasCapture():
			#ending mouse event
			self.help_panel.ReleaseMouse()

	#Function for closing the binded component (stop showing and destroying)
	#It is called when left mouse button is pressed (before releasing)
	#Arguments: component name and binding event (generic: self, event)
	#No return value
	def close_popup(self, evt):
		# stop showing popup on main frame
		self.Show(False)
		#call destructor for popup
		self.Destroy()


#Main frame that contains all other components
class MainFrame(wx.Frame):
    #Initialising the frame
    def __init__(self, parent, title):
            #Calling the constructor of the parent class
            super(MainFrame, self).__init__(parent, title=title, size=(1000, 600))
            #setting the frame in the centre of the screen
            self.Centre()
            #maiking a menu bar for workspace choosing and quitting
            menu_bar = wx.MenuBar()
            #making a menu (future File in menu bar)
            file_menu = wx.Menu()
	    #adding item: Help in menu File
            file_item_help = file_menu.Append(1, 'Help', 'Display additonal information')
            #adding item: Quit in menu File
            file_item_quit = file_menu.Append(wx.ID_EXIT, 'Quit', 'Quit application')
            #adding menu File in menu bar
            menu_bar.Append(file_menu, '&File')
            #setting menu bar
            self.SetMenuBar(menu_bar)
            #binding the event: click on item quit with function close_program
            self.Bind(wx.EVT_MENU, self.close_program, file_item_quit)
            #binding the event: click on item help with function OnHelp
            self.Bind(wx.EVT_MENU, self.OnHelp, file_item_help)
            #making frame visible
            self.Show()
            #setting the frame size to be screen size
            self.Maximize(True)
            #settng the initial value of workspace
            self.workspace = ''
            #settng the initial value of number of executed lines
            self.executed = 0
            #settng the initial value of number of executable lines
            self.executable = 0
            #settng the initial value of coverage percent
            self.coverage = 0
            #making a spliiter in order to devide the frame
            self.splitter = wx.SplitterWindow(self, -1)
            #setting minimum size of future frame parts
            self.splitter.SetMinimumPaneSize(400)
            #defining left panel in left part of the frame
            left_panel = wx.Panel(self.splitter, -1)
            #defining a sizer for left panel
            left_box = wx.BoxSizer(wx.VERTICAL)
            #defining in left panel a tree structure that will hold the names of gcov files
            self.tree = wx.TreeCtrl(left_panel, 1, wx.DefaultPosition, (-1, -1), wx.TR_HAS_BUTTONS)
            #binding the event: tree expanding with function tree_expand
            wx.EVT_TREE_ITEM_EXPANDING(self.tree, self.tree.GetId(), self.tree_expand)
            #binding the event: item selecting with function display_report
            self.Bind(wx.EVT_TREE_SEL_CHANGED, self.display_report)
            #setting the sizer in left panel
            left_panel.SetSizer(left_box)
            #defining right panel in right part of the frame
            right_panel = wx.Panel(self.splitter, -1)
            #defining a sizer for right panel
            right_box = wx.BoxSizer(wx.VERTICAL)
            #defining in right panel a text field that will hold the content of a choosen gcov file
            #option style is set so it can allow scrolling and some text showing posibillities
            self.report_display = wx.TextCtrl(right_panel, -1, '', style = wx.TE_RICH | wx.TE_MULTILINE | wx.VSCROLL)
            #defining in left panel a button for generating gcov and function files
            self.report_button = wx.Button(left_panel, 1, 'GENERATE GCOV AND FUNCTION FILES', size = (400,50))
            #binding the event: click on the button with function report_button_click
            self.report_button.Bind(wx.EVT_BUTTON, self.report_button_click)
            #defining in left panel a button for choosing workpsace 
            self.workspace_choosing_button = wx.Button(left_panel, 1, 'CHOOSE WORKSPACE', size = (400,50)) 
            #binding the event: click on the button with function workspace_choosing_button
            self.workspace_choosing_button.Bind(wx.EVT_BUTTON, self.workspace_choosing_button_click) 
            #defining in left panel a button for dumping coverage data 
            self.dump_coverage_button = wx.Button(left_panel, 2, 'DUMP COVERAGE DATA' ,size = (400,50)) 
            self.dump_coverage_button.Bind(wx.EVT_BUTTON, self.dump_coverage_button_click) 
            #setting font for all static text
            self.font = wx.Font(12, wx.ROMAN, wx.ITALIC, wx.NORMAL) 
            #defining in left panel txt field for process name 
            self.process_name_display = wx.TextCtrl(left_panel, 3, '',size = (400,50), style = wx.TE_RICH) 
            self.process_name_display.SetFont(self.font) 
            #Label containing information about below text filed
            self.info_process_name = wx.StaticText(left_panel,4,"Please enter the name of process \n you wish to dump data for",style = wx.ALIGN_CENTER) 
            self.info_process_name.SetFont(self.font)  
            #defining in left panel a button for generating total project coverage 
            self.total_report_button = wx.Button(left_panel, 1, 'GENERATE TOTAL PROJECT COVERAGE', size = (400,50)) 
            #binding the event: click on the button with function total_report_button_click 
            self.total_report_button.Bind(wx.EVT_BUTTON, self.total_report_button_click) 
            #Shaping box, label and edit fied regarding number of executed lines 
            self.executed_hbox = wx.BoxSizer(wx.HORIZONTAL) 
            self.executed_info = wx.StaticText(left_panel, -1, "Total executed lines:    ") 
            self.executed_info.SetFont(self.font)  
            self.executed_hbox.Add(self.executed_info, 1, wx.EXPAND|wx.ALIGN_LEFT|wx.ALL,10) 
            self.executed_display = wx.TextCtrl(left_panel, 1, '',size = (100,30), style = wx.TE_RICH) 
            self.executed_hbox.Add(self.executed_display,1,wx.EXPAND|wx.ALIGN_LEFT|wx.ALL,10) 
            #Shaping box, label and edit fied regarding number of executable lines 
            self.executable_hbox = wx.BoxSizer(wx.HORIZONTAL) 
            self.executable_info = wx.StaticText(left_panel, -1, "Total executable lines: ") 
            self.executable_info.SetFont(self.font)  
            self.executable_hbox.Add(self.executable_info, 1, wx.EXPAND|wx.ALIGN_RIGHT|wx.ALL,10) 
            self.executable_display = wx.TextCtrl(left_panel, 1, '',size = (100,30), style = wx.TE_RICH) 
            self.executable_hbox.Add(self.executable_display,1,wx.EXPAND|wx.ALIGN_RIGHT|wx.ALL,10) 
            #Shaping box, label and edit fied regarding coverage percent 
            self.coverage_hbox = wx.BoxSizer(wx.HORIZONTAL) 
            self.coverage_info = wx.StaticText(left_panel, -1, "Total code coverage:   ") 
            self.coverage_info.SetFont(self.font)  
            self.coverage_hbox.Add(self.coverage_info, 1, wx.EXPAND|wx.ALIGN_RIGHT|wx.ALL,10) 
            self.coverage_display = wx.TextCtrl(left_panel, 1, '',size = (100,30), style = wx.TE_RICH) 
            self.coverage_hbox.Add(self.coverage_display,1,wx.EXPAND|wx.ALIGN_RIGHT|wx.ALL,10) 
            #adding components defined (tree, button and text field) in propper sizers and in propper order
            right_box.Add(self.report_display, -1, wx.EXPAND)
            left_box.Add(self.workspace_choosing_button) 
            left_box.Add(self.info_process_name) 
            left_box.Add(self.process_name_display) 
            left_box.Add(self.dump_coverage_button) 
            left_box.Add(self.report_button)
            left_box.Add(self.tree, 1, wx.EXPAND)
            left_box.Add(self.total_report_button) 
            left_box.Add(self.executed_hbox) 
            left_box.Add(self.executable_hbox) 
            left_box.Add(self.coverage_hbox) 
            #setting the sizer in right panel
            right_panel.SetSizer(right_box)
            #splitting the frame on: left and right panel
            self.splitter.SplitVertically(left_panel, right_panel)


    #Function for displaying additional information about usage.
    #It is called when menu item: Help is clicked
    #Arguments: component name and binding event (generic: self, event)
    #No return value
    def OnHelp(self, event):
	self.popup_window = HelpPopup(self, wx.SIMPLE_BORDER)
        self.popup_window.Show(True)

    #Function for workspace choosing.
    #It is called when button Choose workspace is clicked
    #Arguments: component name and binding event (generic: self, event)
    #No return value
    def workspace_choosing_button_click(self, event):
        #opening a directory choosing dialog
		workspace_choosing_dialog = wx.DirDialog(self, "Choose workspace")
        #if it is chosen...
		if workspace_choosing_dialog.ShowModal() == wx.ID_OK:
            #saving the path to the choosen directory in variable workspace
			self.workspace = workspace_choosing_dialog.GetPath()
			#deleting old tree
        	self.tree.DeleteAllItems()
        	#building new tree
        	self.build_tree(self.workspace)
        #closing the directory choosing dialog
		workspace_choosing_dialog.Destroy()

    #Function for generating gcov and fun files
    #It is called in function report_button_click
    #Arguments: component name and workspace name (generic: component name is self)
    #No return value
    def generate_reports(self, dir):
        #regular expression for gcda files (gcda-gcov data files)
        gcda_regex = re.compile(r'(\w+)((.\w)?)(\.gcda)')
        #recursive search of all gcda files in workspace or it's subdirectories
        #calling gcov system call for each gcda file
        for f in os.listdir(dir):
            if os.path.isdir(dir+"/"+f):
                self.generate_reports(dir+"/"+f)
            elif gcda_regex.match(f):
				notes_file = open( dir + "/" + f[:-5] + ".gcno", 'r')
				content = notes_file.read()
				if content.find(f[:-5]) > 0:
					directory_list = dir.split("/")
 					new_path = ""
					for directory in directory_list :
						if content.find(directory + "/") > 0 and re.search('[a-zA-Z]|[0-9]|_', directory):
							new_path = new_path + "/" + directory
					new_f = new_path + "/" +  f
					if new_path != "":
						if new_path.split("/")[1] != dir.split("/")[1]:
							new_dir = dir.split(new_path)[0]
							os.chdir(new_dir)
							os.system("gcov -f " + new_f[1:] + " > " + f[:-5] + ".fun")
						else:
							os.chdir(dir)
							os.system("gcov -f " + f + " > " + f[:-5] + ".fun")
					else:
						os.chdir(dir)
						os.system("gcov -f " + f + " > " + f[:-5] + ".fun")

    #Function for generating gcov and fun files and refreshing the tree
    #It is called when button Generate gcov and function files is clicked
    #Arguments: component name and binding event (generic: self, e)
    #No return value
    def report_button_click(self,e):
	if os.path.isdir(self.workspace):
		#generating gcov files
		self.generate_reports(self.workspace)
		#deleting old tree
		self.tree.DeleteAllItems()
		#building new tree
		self.build_tree(self.workspace)
	else:
		error_message_dialog = wx.MessageDialog(self, "Please coose a valid workspace", "Error", wx.OK | wx.ICON_ERROR)
		error_message_dialog.ShowModal()
		error_message_dialog.Destroy()
		#print("Please coose a valid workspace") 

    #Function for dumping coverage data
    #It is called in function dump_coverage_button_click
    #Arguments: component name and process name (generic: component name is self)
    #No return value
    def signal_dump(self, proc_name):
        #extract pid from process name
        pids = os.popen("pidof " + proc_name).read().split()
	if len(pids) == 0:
		error_message_dialog = wx.MessageDialog(self, "Chosen process is not running", "Error", wx.OK | wx.ICON_ERROR)
		error_message_dialog.ShowModal()
		error_message_dialog.Destroy()
	else:
		#send signal to each
		for pid in pids:
			os.system("kill -10 " + str(pid))

       
    #Function for dumping coverage data
    #It is called when button Dump coverage data is clicked
    #Arguments: component name and binding event (generic: self, e)
    #No return value
    def dump_coverage_button_click(self,e):
        #calling function that will send signal that trggers data dump
	user_proc_name = self.process_name_display.GetValue()
	if user_proc_name != "":
		self.signal_dump(user_proc_name)
	else:
		error_message_dialog = wx.MessageDialog(self, "Please specify a proccess name you with to dump data", "Error", wx.OK | wx.ICON_ERROR)
		error_message_dialog.ShowModal()
		error_message_dialog.Destroy()
	

    #Function for parsing single gcov files and gathering total numbers of executed and executable lines
    #It is called in function calculate_totals
    #Arguments: component name and gcov file name (generic: component name is self)
    #Returns numbers of executed and executable lines in gcov file from argument
    def parse_gcov(self, gcov):
	gcov_file = open(gcov,"r")
	lines = gcov_file.readlines()

	loc_executable = 0
	loc_executed = 0

	for line in lines:
		counter = line.split(":")[0].strip()
		if re.match("[0-9]+", counter):
			loc_executable += 1
			loc_executed += 1
		elif re.match("#+", counter):
			loc_executable += 1

	return loc_executed, loc_executable

    #Function for gathering total numbers of executed and executable lines
    #It is called in function total_report_button_click
    #Arguments: component name and workspace name (generic: component name is self)
    #No return value
    def calculate_totals(self, dir):
        #regular expression for gcov files
        gcov_regex = re.compile(r'(\w+)((.\w)?)(\.gcov)')
        #recursive search of all gcov files in workspace or it's subdirectories
        #parsing each gcov system and updateing numbers
        for f in os.listdir(dir):
            if os.path.isdir(dir+"/"+f):
                self.calculate_totals(dir+"/"+f)
            elif gcov_regex.match(f):
		single_executed, single_executable = self.parse_gcov(dir+"/"+f)
		self.executed += single_executed
		self.executable += single_executable

    #Function generating total project coverage
    #It is called when button Generate total project coverage is clicked
    #Arguments: component name and binding event (generic: self, e)
    #No return value
    def total_report_button_click(self,e):
        #calculating totals
	self.executed = 0
	self.executable = 0
	if os.path.isdir(self.workspace):
		self.calculate_totals(self.workspace)
		if self.executable != 0:
			self.executed_display.SetValue(str(self.executed))
			self.executable_display.SetValue(str(self.executable))
			self.coverage = round((float(self.executed)/self.executable)*100, 4)
			self.coverage_display.SetValue(str(self.coverage)+"%")
		else:
			error_message_dialog = wx.MessageDialog(self, "Your project code does not contain any executable line. Coverage can not be calculated.", "Warrning", wx.OK | wx.ICON_ERROR)
			error_message_dialog.ShowModal()
			error_message_dialog.Destroy()		
	else:
		error_message_dialog = wx.MessageDialog(self, "Please coose a valid workspace", "Warrning", wx.OK | wx.ICON_ERROR)
		error_message_dialog.ShowModal()
		error_message_dialog.Destroy()


    #Function for quiting the program
    #It is called when menu item: Quit is clicked
    #Arguments: component name and binding event (generic: self, e)
    #No return value
    def close_program(self, e):
        self.Close()

    #Function for displaying the content of a gcov or fun file
    #It is called when an item is selected in the tree of names
    #Arguments: component name and binding event (generic: self, event)
    #No return value
    def display_report(self, event):
        #getting the name of selected file
        item = event.GetItem()
        #getting data about selected item
        itemData = self.tree.GetItemData(item).GetData()
        #getting path of selected item
        addr = itemData[0]
        #if it is a directory...
        if os.path.isdir(addr):
                    #reset the display
                    self.report_display.SetValue("")
        #if it is a file (then it is a gcov or fun file)
        else:
                    #open the selected file
                    selected_file = open(addr, 'r')
                    #read the content of selectted file
                    content_original = selected_file.read()
                    #formating the output
                    content_double_space_formated = content_original.replace(' ', '   ')
                    content_start_space_formated = content_double_space_formated.replace('   #', '#')
                    #write the content of the selected file in text field (a.k.a. display)
                    self.report_display.SetValue(content_start_space_formated)

    #Function for checking whether the node has been previously expanded.
    #If not, building out the node, which is then marked as expanded.
    #It is called when the user expands a node on the tree object.
    #Arguments: component name and binding event (generic: self, event)
    #No return value
    def tree_expand(self, event):
        # getting the wxID of the entry to expand and checking it's validity
        item_id = event.GetItem()
        if not item_id.IsOk():
            item_id = self.tree.GetSelection()
        # only build that tree if not previously expanded
        old_pydata = self.tree.GetPyData(item_id)
        if old_pydata[1] == False:
            # clean the subtree and rebuild it
            self.tree.DeleteChildren(item_id)
            self.extend_tree(item_id)
            self.tree.SetPyData(item_id,(old_pydata[0], True))

    #Function for building the tree of gcov and fun file names
    #Arguments: component name and name (generic: component name is self)
    #No return value
    def build_tree(self, rootdir):
        #Add a new root element and then its children
        self.root_id = self.tree.AddRoot(rootdir)
        self.tree.SetPyData(self.root_id, (rootdir,1))
        self.extend_tree(self.root_id)
        self.tree.Expand(self.root_id)

    #recursive search of files that match regular expression regex in path or it's subdirectories
    #returns the number of hits
    #Arguments: component name, absolute path to the directory in wich we search,
    	#regular expression we try to match, variable that will store the number of found files
    #Return value is the number of files found
    def recursive_search(self, path, regex,value):
            for f in os.listdir(path):
                    if os.path.isdir(path+"/"+f):
                            value = self.recursive_search(path+"/"+f, regex, value)
                    elif regex.match(f):
                            value+=1
            return value

    #Function for adding the children in the tree
    #A child is added if it is a gcov file, or directory in the absolute path of some gcov or fun file
    #Arguments: component name and workspace name (generic: component name is self)
    #No return value
    def extend_tree(self, parent_id):
        #setting root and regular expression for gcov and fun files
        parent_dir = self.tree.GetPyData(parent_id)[0]
        report_regex = re.compile(r'(\w+)((.\w)?)((\.gcov)|(\.fun))')
        #recursive search
        sub_directories = dircache.listdir(parent_dir)
        sub_directories.sort()
        for child in sub_directories:
            child_path = os.path.join(parent_dir,child)
            #it must be a directory in the absolute path of some gcov file...
            if os.path.isdir(child_path) and self.recursive_search(child_path, report_regex, 0) and not os.path.islink(child):
                child_id = self.tree.AppendItem(parent_id, child)
                self.tree.SetPyData(child_id, (child_path, False))
                new_parent = child
                new_parent_id = child_id
                new_parent_path = child_path
                new_sub_directories = dircache.listdir(new_parent_path)
                new_sub_directories.sort()
                for grandchild in new_sub_directories:
                    grandchild_path = os.path.join(new_parent_path,grandchild)
                    if os.path.isdir(grandchild_path) and self.recursive_search(child_path, report_regex, 0) and not os.path.islink(grandchild_path):
                        grandchild_id = self.tree.AppendItem(new_parent_id, grandchild)
                        self.tree.SetPyData(grandchild_id, (grandchild_path,False))
                    elif report_regex.match(grandchild):
                        grandchild_id = self.tree.AppendItem(new_parent_id, grandchild)
                        self.tree.SetPyData(grandchild_id, (grandchild_path,False))
            #or a gcov file itself
            elif report_regex.match(child):
                child_id = self.tree.AppendItem(parent_id, child)
                self.tree.SetPyData(child_id, (child_path, False))

#Main function
if __name__ == "__main__":
    #defining application
    app = wx.PySimpleApp(0)
    #initializing all available image handlers
    wx.InitAllImageHandlers()
    #defining the frame
    frame = MainFrame(None, "")
    frame.SetTitle('Code Coverage Viewer')
    #setting the frame
    app.SetTopWindow(frame)
    #making frame visible
    frame.Show()
    #starting the application (the program)
    app.MainLoop()
