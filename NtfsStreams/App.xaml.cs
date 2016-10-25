using NtfsStreams.ViewModels;
using System;
using System.Collections.Generic;
using System.ComponentModel.Composition.Hosting;
using System.Configuration;
using System.Data;
using System.Linq;
using System.Reflection;
using System.Threading.Tasks;
using System.Windows;
using Zodiacon.WPF;

namespace NtfsStreams {
	/// <summary>
	/// Interaction logic for App.xaml
	/// </summary>
	public partial class App : Application {
		MainViewModel _mainViewModel;

		protected override void OnStartup(StartupEventArgs e) {
			var catalog = new AggregateCatalog(
				new AssemblyCatalog(Assembly.GetExecutingAssembly()),
				new AssemblyCatalog(typeof(IDialogService).Assembly));
			var container = new CompositionContainer(catalog);

			var vm = container.GetExportedValue<MainViewModel>();
			_mainViewModel = vm;
			var win = new MainWindow { DataContext = vm };
			vm.MessageBoxService.SetOwner(win);
			win.Show();
		}

		protected override void OnExit(ExitEventArgs e) {
			_mainViewModel.Close();
			base.OnExit(e);
		}
	}
}
