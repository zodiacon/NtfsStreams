using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Input;

namespace NtfsStreams.ViewModels {
	class FolderViewModel : TabViewModelBase {
		public override string Icon => "/icons/folder_document.ico";

		public override string Title => Folder.Length > 50 ? "..." + Folder.Substring(Folder.Length - 50) : Folder;

		public string Folder { get; }

		ObservableCollection<FileStreamsViewModel> _files;

		public IList<FileStreamsViewModel> Files => _files;

		MainViewModel _mainViewModel;

		public FolderViewModel(MainViewModel mainVieModel, string folder, params FileStreamsViewModel[] files) {
			_mainViewModel = mainVieModel;
			Folder = folder;
			_files = new ObservableCollection<FileStreamsViewModel>(files.OrderBy(file => Path.GetFileName(file.Path)));
		}

		private FileStreamsViewModel[] _selectedFiles;

		public FileStreamsViewModel[] SelectedFiles {
			get { return _selectedFiles; }
			set { SetProperty(ref _selectedFiles, value); }
		}

		public void OpenSelectedFiles(MainViewModel vm) {
			TabViewModelBase first = null;
			foreach (var file in SelectedFiles) {
				var tab = vm.AddTab(file);
				if (first == null)
					first = tab;
			}
			vm.SelectedTab = first;
		}

		public ICommand ViewFilesCommand => _mainViewModel.ViewFilesCommand;
	}
}
