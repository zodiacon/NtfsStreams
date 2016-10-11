using Prism.Commands;
using Prism.Mvvm;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.ComponentModel.Composition;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using Zodiacon.WPF;
using static NtfsStreams.NativeMethods;

namespace NtfsStreams.ViewModels {
	class MainViewModel : BindableBase {
		public string Title => Constants.Title + (Helpers.IsAdmin ? " (Administrator)" : string.Empty);

		public DelegateCommand OpenFolderCommand { get; }
		public DelegateCommand OpenFileCommand { get; }

		public IFileDialogService FileDialogService = UIServicesDefaults.FileDialogService;
		public IMessageBoxService MessageBoxService = UIServicesDefaults.MessageBoxService;

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
			}
		}

		public MainViewModel() {
			OpenFolderCommand = new DelegateCommand(() => {
			});

			OpenFileCommand = new DelegateCommand(() => {
				var file = BrowseForFile();
				if (file == null) return;

				try {
					var fileStreams = FindStreams(file);
				}
				catch (Win32Exception ex) {
					if (ex.ErrorCode == 38)
						MessageBoxService.ShowMessage("No streams in file.", Constants.Title);
					else
						MessageBoxService.ShowMessage($"Error: {ex.Message}", Constants.Title);
				}
			});

		}

		private FileStreamsViewModel FindStreams(string filename) {
			StreamFindData data;
			var hFind = FindFirstStreamW(filename, 0, out data);
			if (hFind == new IntPtr(-1))
				throw new Win32Exception(Marshal.GetLastWin32Error());
			var file = new FileStreamsViewModel();
			do {
				file.Streams.Add(new StreamViewModel {
					Name = data.StreamName,
					Size = data.StreamSize
				});
			} while (FindNextStreamW(hFind, out data));

			FindClose(hFind);
			return file;
		}

		private string BrowseForFile() {
			var file = FileDialogService.GetFileForOpen();
			return file;
		}
	}
}
