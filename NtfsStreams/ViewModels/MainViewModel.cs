using Prism.Commands;
using Prism.Mvvm;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.ComponentModel.Composition;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Input;
using WPFFolderBrowser;
using Zodiacon.WPF;
using static NtfsStreams.NativeMethods;

namespace NtfsStreams.ViewModels {
	class MainViewModel : BindableBase {
		ObservableCollection<TabViewModelBase> _tabs = new ObservableCollection<TabViewModelBase>();

		public string Title => Constants.Title + (Helpers.IsAdmin ? " (Administrator)" : string.Empty);

		public IList<TabViewModelBase> Tabs => _tabs;

		public DelegateCommandBase OpenFolderCommand { get; }
		public DelegateCommandBase OpenFileCommand { get; }
		public DelegateCommandBase ViewFilesCommand { get; }
		public DelegateCommandBase CloseTabCommand { get; }

		public IFileDialogService FileDialogService = UIServicesDefaults.FileDialogService;
		public IMessageBoxService MessageBoxService = UIServicesDefaults.MessageBoxService;

		private TabViewModelBase _selectedTab;

		public TabViewModelBase SelectedTab {
			get { return _selectedTab; }
			set { SetProperty(ref _selectedTab, value); }
		}

		public IEnumerable<object> ToolbarItems {
			get {
				yield return new {
					Text = "Open File...",
					Icon = "/icons/file_view.ico",
					Command = OpenFileCommand
				};

				yield return new {
					Text = "Open Folder...",
					Icon = "/icons/folder_view.ico",
					Command = OpenFolderCommand
				};
				yield return new {
					Text = "View Files",
					Icon = "/icons/view.ico",
					Command = ViewFilesCommand
				};
			}
		}

		public ICommand ExitCommand { get; } = new DelegateCommand(() => Application.Current.Shutdown());

		public MainViewModel() {
			OpenFolderCommand = new DelegateCommand(() => {
				var folder = BrowseForFolder();
				if (folder == null) return;

				int total = 0;
				var files = new List<FileStreamsViewModel>();
				foreach (var filename in Directory.EnumerateFiles(folder)) {
					total++;
					try {
						var file = FindStreams(filename);
						if (file == null)
							continue;

						files.Add(file);
					}
					catch (Win32Exception) {
					}
				}

				if (files.Count == 0)
					MessageBoxService.ShowMessage($"No streams found in any of the {total} files.", Constants.Title, MessageBoxButton.OK, MessageBoxImage.Information);
				else {
					var folderViewModel = new FolderViewModel(this, folder, files.ToArray());
					//Tabs.Add(folderViewModel);
					var tab = AddTab(folderViewModel);
					SelectedTab = tab; 
				}
			});

			OpenFileCommand = new DelegateCommand(() => {
				var file = BrowseForFile();
				if (file == null) return;

				try {
					var fileStreams = FindStreams(file);
					if (fileStreams == null) {
						MessageBoxService.ShowMessage("No alternate streams in file.", Constants.Title, MessageBoxButton.OK, MessageBoxImage.Information);
						return;
					}
					var tab = AddTab(fileStreams);
					SelectedTab = tab;
				}
				catch (Win32Exception ex) {
					MessageBoxService.ShowMessage(ex.Message, Constants.Title, MessageBoxButton.OK, MessageBoxImage.Error);
				}
			});

			ViewFilesCommand = new DelegateCommand(() => {
				(SelectedTab as FolderViewModel).OpenSelectedFiles(this);
			}, () => SelectedTab is FolderViewModel).ObservesProperty(() => SelectedTab);

			CloseTabCommand = new DelegateCommand<TabViewModelBase>(tab => Tabs.Remove(tab));
		}

		public ICommand CloseAllCommand => new DelegateCommand(() => Tabs.Clear(), () => Tabs.Count > 0)
			.ObservesProperty(() => SelectedTab);

		public ICommand CloseAllButThisCommand => new DelegateCommand(() => {
			var tab = SelectedTab;
			Tabs.Clear();
			AddTab(tab);
			SelectedTab = tab;
		}, () => SelectedTab != null && Tabs.Count > 1).ObservesProperty(() => SelectedTab);

		public TabViewModelBase AddTab(TabViewModelBase newTab) {
			var tab = _tabs.FirstOrDefault(t => t.Title == newTab.Title);
			if (tab == null)
				Tabs.Add(tab = newTab);
			return tab;
		}

		string BrowseForFolder() {
			var dlg = new WPFFolderBrowserDialog {
				Title = "Select Folder",
			};
			if (dlg.ShowDialog() == true) {
				return dlg.FileName;
			}
			return null;
		}

		private FileStreamsViewModel FindStreams(string filename) {
			StreamFindData data;
			var hFind = FindFirstStreamW(filename, 0, out data);
			if (hFind == new IntPtr(-1))
				throw new Win32Exception(Marshal.GetLastWin32Error());
			var file = new FileStreamsViewModel(filename);
			do {
				if (data.StreamName == "::$DATA")
					continue;

				file.Streams.Add(new StreamViewModel {
					StreamName = data.StreamName,
					StreamSize = data.StreamSize
				});
			} while (FindNextStreamW(hFind, out data));

			FindClose(hFind);
			if (file.Streams.Count == 0)
				return null;

			return file;
		}

		private string BrowseForFile() {
			var file = FileDialogService.GetFileForOpen();
			return file;
		}
	}
}
