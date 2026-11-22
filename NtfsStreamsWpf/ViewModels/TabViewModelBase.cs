using Prism.Mvvm;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace NtfsStreams.ViewModels {
	abstract class TabViewModelBase : BindableBase {
		public abstract string Icon { get; }
		public abstract string Title { get; }
	}
}
