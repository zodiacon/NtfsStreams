using Prism.Mvvm;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using static NtfsStreams.NativeMethods;

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

			var hFile = CreateFile(Path + stream.StreamName, GENERIC_READ, FILE_SHARE_READ, IntPtr.Zero, OPEN_EXISTING, 0, IntPtr.Zero);
			if (hFile.IsInvalid) {
				return null;
			}
			else {
				var size = Math.Min(1 << 16, stream.StreamSize);	// read 64KB at most
				byte[] bytes = new byte[size];
				using (var fs = new FileStream(hFile, FileAccess.Read)) {
					fs.Read(bytes, 0, bytes.Length);
				}
				return bytes;
			}
		}
	}
}
