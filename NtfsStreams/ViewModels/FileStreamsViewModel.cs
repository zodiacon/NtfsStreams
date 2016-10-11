using Prism.Mvvm;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace NtfsStreams.ViewModels {
	class FileStreamsViewModel : BindableBase {
		ObservableCollection<StreamViewModel> _streams = new ObservableCollection<StreamViewModel>();

		public IList<StreamViewModel> Streams => _streams;
	}
}
