using Prism.Mvvm;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace NtfsStreams.ViewModels {
	class FileStreamsViewModel : TabViewModelBase {
		ObservableCollection<StreamViewModel> _streams = new ObservableCollection<StreamViewModel>();

		public IList<StreamViewModel> Streams => _streams;

		public BinaryViewModel DataViewModel { get; } = new BinaryViewModel();

		public string Path { get; }

		public override string Icon => "/icons/file.ico";

		public override string Title => System.IO.Path.GetFileName(Path);

		public FileStreamsViewModel(string path) {
			Path = path;
		}

		public string FileName => Title;

		private StreamViewModel _selectedStream;

		public StreamViewModel SelectedStream {
			get { return _selectedStream; }
			set {
				if (SetProperty(ref _selectedStream, value) && value != null) {
					DataViewModel.Data = ReadStreamData(SelectedStream);
				}
			}
		}

		private byte[] ReadStreamData(StreamViewModel stream) {
			if (stream == null) return null;

			var hFile = NativeMethods.CreateFile(Path + stream.StreamName, 0x80000000, 1, IntPtr.Zero, 3, 0, IntPtr.Zero);
			if (hFile.IsInvalid) {
				return null;
			}
			else {
				byte[] bytes = new byte[stream.StreamSize];
				using (var fs = new FileStream(hFile, FileAccess.Read)) {
					fs.Read(bytes, 0, bytes.Length);
				}
				return bytes;
			}
		}
	}
}
