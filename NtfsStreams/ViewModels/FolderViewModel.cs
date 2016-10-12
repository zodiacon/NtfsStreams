using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace NtfsStreams.ViewModels {
	class FolderViewModel : TabViewModelBase {
		public override string Icon => "/icons/folder_document.ico";

		public override string Title => Folder.Length > 50 ? "..." + Folder.Substring(Folder.Length - 50) : Folder;

		public string Folder { get; }

		ObservableCollection<FileStreamsViewModel> _files;

		public IList<FileStreamsViewModel> Files => _files;

		public FolderViewModel(string folder, params FileStreamsViewModel[] files) {
			Folder = folder;
			_files = new ObservableCollection<FileStreamsViewModel>(files.OrderBy(file => Path.GetFileName(file.Path)));
		}

		private FileStreamsViewModel[] _selectedFiles;

		public FileStreamsViewModel[] SelectedFiles {
			get { return _selectedFiles; }
			set { SetProperty(ref _selectedFiles, value); }
		}

		public void OpenSelectedFiles(MainViewModel vm) {
			FileStreamsViewModel first = null;
			foreach (var file in SelectedFiles) {
				vm.Tabs.Add(file);
				if (first == null)
					first = file;
			}
			vm.SelectedTab = first;
		}
	}
}
