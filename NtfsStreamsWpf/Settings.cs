using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace NtfsStreams {
	public class Settings {
		public ObservableCollection<string>	RecentFiles { get; set; }
		public ObservableCollection<string> RecentFolders { get; set; }
	}
}
